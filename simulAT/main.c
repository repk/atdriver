#define _XOPEN_SOURCE
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <sys/select.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

#define SERIAL_PORT "/dev/ptmx"

#define DBG

#ifdef DBG
#define DBGMSG(...) printf("----> dbg: " __VA_ARGS__)
#else
#define DBGMSG(...)
#endif

#define MAX(a, b) (((a) > (b)) ? (a) : (b))

/**
 * New sms notification : 
 *
 * AT+CNMI?
 * +CNMI: 2,1,0,0,0 -> Wait for TE link ready and flush messages
 *
 * OK
 *
 * +CMTI: "SM",4 -> Notification message is at index 4 of sim
 * AT+CMGR=4 -> get message at index 4
 * +CMGR: "REC UNREAD","+33686108660","","14/01/27,21:25:54+04"
 * Prout 
 *
 * OK
 */


static volatile int cont = 1;
static int pf[2];

static void sigint(int i)
{
	(void)i;
	cont = 0;
}

static void sigusr1(int i)
{
	(void)i;
	write(pf[1], "\n", 1);
}

#define NUMSZ 12
#define XSTR(s) STR(s)
#define STR(s) #s

#define NFMT "%." XSTR(NUMSZ) "s"

struct sms {
	struct sms *next;
	size_t len;
	char num[NUMSZ];
	char data[];
};

struct sms_storage {
	struct sms *ss_list;
	size_t ss_maxidx;
};


static inline void sms_storage_init(struct sms_storage *sto)
{
	sto->ss_list = NULL;
	sto->ss_maxidx = 0;
}

static inline int sms_storage_add(struct sms_storage *sto,
		char *num, char *txt)
{
	struct sms *s, **prev;

	s = malloc(sizeof(*s) + strlen(txt) + 1);
	if(s == NULL)
		return -1;

	s->len = strlen(txt);
	strncpy(s->num, num, NUMSZ);
	strcpy(s->data, txt);
	s->next = NULL;

	for(prev = &sto->ss_list; *prev != NULL; prev = &((*prev)->next));

	*prev = s;

	++sto->ss_maxidx;

	return sto->ss_maxidx;
}

#define SMSFMT								\
	"+CMGL: 2,\"REC UNREAD\",\"" NFMT "\",,\"07/02/18,00:05:10+32\r\n%s\r\n"
#define SMSFMTSZ (sizeof(SMSFMT) - sizeof(NFMT) - 2)

static inline int sms_storage_get(struct sms_storage *sto, size_t idx,
		char *buf, size_t len)
{
	struct sms *s;
	size_t i, l;

	if(idx > sto->ss_maxidx)
		return -1;

	s = sto->ss_list;
	for(i = 0; i < idx - 1; ++i)
		s = s->next;

	l = snprintf(buf, len, SMSFMT, s->num, s->data);

	if(l == len)
		return -1;

	return l;
}

static inline void sms_storage_cleanup(struct sms_storage *sto)
{
	struct sms *s, *next;

	s = sto->ss_list;

	while(s != NULL) {
		next = s->next;
		free(s);
		s = next;
	}

	sms_storage_init(sto);
}



static void send_new_sms(int fd, struct sms_storage *sto)
{
	static int n = 0;
	char buf[256], rsp[256];
	int newidx;
	char dummy[1];
	read(pf[0], dummy, 1);
	(void)dummy;

	snprintf(buf, 256, "Sms notification n%u", n);

	newidx = sms_storage_add(sto, "+33888888888", buf);
	if(newidx < 0)
		return;

	snprintf(rsp, 256, "+CMTI: \"SM\",%u\r\n", newidx);
	write(fd, rsp, strlen(rsp));
    ++n;
}

static int newsms;
static char num[20];

static inline void rcv_cmd(int fd, struct sms_storage *sto, char *cmd)
{
	char buf[256];
	size_t i, idx;
	int len;

	if(strcmp(cmd, "\r\n") == 0) {
		DBGMSG("Respond to connection test AT command...\n");
		write(fd, "OK\r\n", sizeof("OK\r\n") - 1);
	} else if(strcmp(cmd, "+CMGF=1\r\n") == 0) {
		DBGMSG("Pass into sms text mode...\n");
		write(fd, "OK\r\n", sizeof("OK\r\n") - 1);
	} else if(sscanf(cmd, "+CMGS=\"%s\"\r\n", num) == 1) {
		DBGMSG("Prepare new sms for number %12s...\n", num);
		newsms = 1;
		write(fd, ">", 1);
	} else if(strcmp(cmd, "+CMGL=\"ALL\"\r\n") == 0) {
		DBGMSG("Sending all stored sms...\n");
		for(i = 0; i < sto->ss_maxidx; ++i) {
			len = sms_storage_get(sto, i + 1, buf, 256);
			if(len < 0)
				continue;
			DBGMSG("\tSending one sms...\n");
			write(fd, buf, len);
		}
		DBGMSG("Ending sms list...\n");
		write(fd, "OK\r\n", sizeof("OK\r\n") - 1);
	} else if(sscanf(cmd, "+CMGR=%zu", &idx) == 1) {
		DBGMSG("Trying to send sms at index %zu...\n", idx);
		len = sms_storage_get(sto, idx, buf, 256);
		if(len > 0) {
			DBGMSG("\tOne sms found, sending...\n");
			write(fd, buf, len);
			write(fd, "OK\r\n", sizeof("OK\r\n") - 1);
		}
	} else {
		printf("Unknow command : %s\n", cmd);
	}
}


static void rcv_msg(int fd, struct sms_storage *sto)
{
	char rcv[256] = {};
	char *cmd;
	size_t n;

	n = read(fd, rcv, 255);
	if(n <= 0)
		return;

	if(newsms == 1) {
		DBGMSG("Receiving sms data ...\n");
		printf("Sending sms to %s : %s\n", num, rcv);
		if(strchr(rcv, 26) != NULL) {
			newsms = 0;
			write(fd, "OK\r\n", sizeof("OK\r\n") - 1);
		}
		return;
	}

	if(n < sizeof("AT\r\n") - 1)
		return;

	if(strncmp(rcv, "AT", sizeof("AT") - 1) != 0)
		return;

	if(strncmp(rcv + n - 2, "\r\n", sizeof("\r\n") - 1) != 0)
		return;


	/* skip AT */
	cmd = strtok(rcv + 2, ";");
	if(cmd == NULL)
		return;

	rcv_cmd(fd, sto, cmd);
	while((cmd = strtok(NULL, ";"))) {
		if(strcmp(cmd, "\r\n") != 0)
			rcv_cmd(fd, sto, cmd);
	}

}


int main(void)
{
	int fd;
	char name[256];
	struct termios options;
	fd_set fset;
	struct sms_storage sto;
	struct sigaction sig_int = { .sa_handler = &sigint };
	struct sigaction sig_usr = {
		.sa_handler = &sigusr1,
		.sa_flags = SA_RESTART
	};

	sms_storage_init(&sto);
	sms_storage_add(&sto, "+33666666666", "Hey first sms");
	sms_storage_add(&sto, "+33777777777", "Hey second sms");

	if(pipe(pf) != 0) {
		perror("Failed to create a pipe\n");
		return EXIT_FAILURE;
	}

	fd = open(SERIAL_PORT, O_RDWR | O_NOCTTY | O_NDELAY);
	if(fd == -1) {
		perror("Failed to open "SERIAL_PORT"\n");
		return EXIT_FAILURE;
	}

	grantpt(fd);
	unlockpt(fd);

	if(ptsname_r(fd, name, 256) != 0) {
		perror("Failed to get Pseudo terminal slave name : ");
		close(fd);
		return EXIT_FAILURE;
	}

	printf("Pseudo terminal %s created\n", name);

	tcgetattr(fd, &options);
	cfsetispeed(&options, B19200);
	cfsetospeed(&options, B19200);
	tcsetattr(fd, TCSANOW, &options);
	printf("Setting Baud rate to : 19200 \n");

	sigaction(SIGINT, &sig_int, NULL);
	sigaction(SIGUSR1, &sig_usr, NULL);

	while(cont) {
		FD_ZERO(&fset);
		FD_SET(fd, &fset);
		FD_SET(pf[0], &fset);
		if(select(MAX(pf[0], fd) + 1, &fset, NULL, NULL, NULL) < 0) {
			if(errno != EINTR)
				perror("Select failed");
			continue;
		}

		if(FD_ISSET(pf[0], &fset))
			send_new_sms(fd, &sto);

		if(FD_ISSET(fd, &fset))
			rcv_msg(fd, &sto);

	};

	close(fd);
	sms_storage_cleanup(&sto);

	return EXIT_SUCCESS;
}
