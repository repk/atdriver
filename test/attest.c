#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>


#define N_ATSMS 25

#define SMS "+33666666666\nHey buddy boy whatsup"
#define BADPIN "PIN=121221"
#define GOODPIN "PIN=1234"

#define STRLEN(s) (sizeof(s) - 1)

char SMSDELIM[2] = {26};


void usage(char const *prog)
{
	fprintf(stderr, "Usage: %s <tty>\n", prog);
}

int main(int argc, char **argv)
{
	int ldisc = N_ATSMS;
	int fd;
	char *parse;
	char rcv[256] = {};
	struct termios options;


	if(argc != 2) {
		usage(argv[0]);
		return -EXIT_FAILURE;
	}


	fd = open(argv[1], O_RDWR | O_NOCTTY | O_NDELAY);
	if(fd == -1) {
		fprintf(stderr, "Failed to open %s", argv[1]);
		perror("");
		return EXIT_FAILURE;
	}

	if(ioctl(fd, TIOCSETD, &ldisc) < 0) {
		perror("Failed to change line discipline");
		return EXIT_FAILURE;
	}

	tcgetattr(fd, &options);
	cfsetispeed(&options, B19200);
	cfsetospeed(&options, B19200);
	options.c_lflag |= ICANON;
	tcsetattr(fd, TCSANOW, &options);
	printf("Setting line discipline and baud rate to : 19200\n");

	sleep(3);

	printf("----> Test bad pin : ");
		if(write(fd, BADPIN, STRLEN(BADPIN)) < 0)
			printf("[OK]\n");
		else
			printf("[FAILED]\n");

	printf("----> Test good pin :");
		if(write(fd, GOODPIN, STRLEN(GOODPIN)) > 0)
			printf("[OK]\n");
		else
			printf("[FAILED]\n");

	printf("----> Sending sms test:\n");

	if(write(fd, SMS, STRLEN(SMS)) < 0)
		perror("SMS sending error");


	printf("----> Reading stored sms test:\n");

	if(read(fd, rcv, STRLEN(rcv)) < 0)
		perror("SMS receiving error");
	else {
		parse = strtok(rcv, SMSDELIM);
		if(parse == NULL)
			return EXIT_FAILURE;
		printf("Recv an sms : \n%s \n", parse);
		while((parse = strtok(NULL, SMSDELIM)))
			printf("Recv an sms : \n%s \n", parse);
	}

	memset(rcv, 0, 256);

	printf("----> Reading new incoming sms test:\n");
	/* Wait for incoming sms */
	if(read(fd, rcv, STRLEN(rcv)) < 0)
		perror("SMS receiving error");
	else {
		parse = strtok(rcv, SMSDELIM);
		if(parse == NULL)
			return EXIT_FAILURE;
		printf("Recv an sms : \n%s \n", parse);
		while((parse = strtok(NULL, SMSDELIM)))
			printf("Recv an sms : \n%s \n", parse);
	}

	printf("----> Reading new incoming sms test n2:\n");
	/* Wait for incoming sms */
	if(read(fd, rcv, STRLEN(rcv)) < 0)
		perror("SMS receiving error");
	else {
		parse = strtok(rcv, SMSDELIM);
		if(parse == NULL)
			return EXIT_FAILURE;
		printf("Recv an sms : \n%s \n", parse);
		while((parse = strtok(NULL, SMSDELIM)))
			printf("Recv an sms : \n%s \n", parse);
	}

	close(fd);
	return EXIT_SUCCESS;
}
