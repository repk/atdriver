#include <linux/init.h>
#include <linux/module.h>
#include <linux/tty.h>
#include <linux/tty_ldisc.h>
#include <linux/slab.h> /* Kzalloc */
#include <linux/jiffies.h>
#include <linux/sched.h>
#include <linux/wait.h>

/* copy_to_user */
#include <linux/uaccess.h>

#include "n_atsms.h"

#define STRLEN(s) (sizeof(s) - 1)

#define ATS_NUMLEN 12
#define ATS_SMSLEN 160

#define ATS_MSGLEN (ATS_NUMLEN + 1 + ATS_SMSLEN)

#define ATSMS_TIMEMOUTMS 10000
#define ATSMS_EVENT_TIMEOUT (msecs_to_jiffies(ATSMS_TIMEMOUTMS))

#define ATSMSBUF_LEN 512
struct atsms_buf {
	size_t atsb_count;
	char atsb_rxbuf[ATSMSBUF_LEN];
};



#define ATSRM_SMSLEN_GET(a) (sizeof((a)->atsrm_num) + 1 + (a)->atsrm_len)
struct atsms_rcvmsg {
	struct list_head atsrm_next;
	char atsrm_num[ATS_NUMLEN];
	char atsrm_msg[ATS_SMSLEN];
	size_t atsrm_len; /* Total sms text lenght without number */
};



struct atsms_cmd {
	struct list_head atsc_next;
	size_t atsc_len;
	char atsc_data[];
};



#define ATSRB_INIT		0
#define ATSRB_SMS_FETCHED	1
#define ATSRB_SMS_SYNC		2


/**
 * XXX Do we really need the following structure
 */
struct atsms_rcvbox {
	struct list_head atsrb_msglist;
	struct list_head atsrb_cmdlist;
	unsigned int atsrb_state;
};


#define ATSMS_STATE_INIT	0
#define ATSMS_STATE_OK		1
#define ATSMS_STATE_CHANGE_MODE	2
#define ATSMS_STATE_NUM		3
#define ATSMS_STATE_SMSG	4
#define ATSMS_STATE_RMSG	5
#define ATSMS_STATE_PIN		6

#define ATSMS_MODE_UNDEF	0
#define ATSMS_MODE_SMSTEXT	1

#define ATSMS_NOERROR		0


struct atsms {
	struct atsms_buf	ats_buf;
	wait_queue_head_t	ats_queue;
	wait_queue_head_t	ats_rdqueue;
	struct atsms_rcvbox	ats_smsbox;
	unsigned int		ats_state;
	unsigned int		ats_mode;
	int			ats_error;
};



#define ATSMS_ERROR_TOKEN	"+CME ERROR:"
#define ATSMS_SMSENTRY_TOKEN	"+CMGL:"

/**
 * Alloc a atsms structure
 */
static inline struct atsms * atsms_alloc_struct(void)
{
	struct atsms *ats = kzalloc(sizeof(struct atsms), GFP_KERNEL);
	if(ats != NULL) {
		INIT_LIST_HEAD(&ats->ats_smsbox.atsrb_msglist);
		INIT_LIST_HEAD(&ats->ats_smsbox.atsrb_cmdlist);
	}
	return ats;
}



/**
 * Free a atsms structure
 */
static inline void atsms_free_struct(struct atsms *ats)
{
	kfree(ats);
}



/**
 * Alloc a new sms structure
 */
static inline struct atsms_rcvmsg * atsms_alloc_rcsmsg(void)
{
	struct atsms_rcvmsg *p = kmalloc(sizeof(*p), GFP_KERNEL);
	if(p != NULL)
		p->atsrm_len = 0;
	return p;
}



/**
 * Free sms structure
 */
static inline void atsms_free_rcvmsg(struct atsms_rcvmsg *sms)
{
	kfree(sms);
}

#define list_tail_entry(ptr, type, member) \
	list_entry((ptr)->prev, type, member)



/**
 * Fill current sms
 */
static inline int atsld_sms_fill(struct atsms *ats, struct atsms_buf *b)
{
	struct list_head *rb = &ats->ats_smsbox.atsrb_msglist;
	struct atsms_rcvmsg *m;
	size_t len;

	ATSMSLD_DBGMSG("New sms data");

	if(list_empty(rb)) {
		return -1;
	}

	/* get last (tail) filling sms */
	m = list_tail_entry(rb, struct atsms_rcvmsg, atsrm_next);

	len = b->atsb_count;
	if(len > ATS_SMSLEN - m->atsrm_len)
		len = ATS_SMSLEN - m->atsrm_len;

	memcpy(m->atsrm_msg + m->atsrm_len, b->atsb_rxbuf, len);
	m->atsrm_len += len;

	return len;
}



/**
 * Adding a new sms in the box
 *
 * TODO maybe add sms receive date
 */
static inline int atsld_sms_new(struct atsms *ats, struct atsms_buf *b)
{
	char *num, *end;
	struct atsms_rcvmsg *new;

	ATSMSLD_DBGMSG("New sms entry");

	end = b->atsb_rxbuf + b->atsb_count - 1;
	num = memchr(b->atsb_rxbuf, ',', b->atsb_count);

	if(num >= end)
		return -1;
	++num;
	num = memchr(num, ',', b->atsb_count  - (num - b->atsb_rxbuf));
	if(num + ATS_NUMLEN + 2 >= end || num[1] != '"')
		return -1;
	num += 2;

	new = atsms_alloc_rcsmsg();
	if(new == NULL)
		return -1;

	memcpy(new->atsrm_num, num, ATS_NUMLEN);
	list_add_tail(&new->atsrm_next, &ats->ats_smsbox.atsrb_msglist);

	return 0;
}



/**
 * Wait for device to answer with success or error code
 */
static inline int atsld_wait(struct atsms *ats)
{
	int res;

	res = wait_event_interruptible_timeout(ats->ats_queue,
			ats->ats_state == ATSMS_STATE_OK, ATSMS_EVENT_TIMEOUT);

	if(res < 0) {
		return -EINTR;
	} else if(res == 0) {
		return -EINVAL;
	}

	return ats->ats_error;
}



/**
 * Wait for new sms to come
 *
 * TODO merge two waiting queues
 */
static inline int atsld_wait_sms(struct atsms *ats)
{
	int res;

	ats->ats_error = ATSMS_NOERROR;

	res = wait_event_interruptible(ats->ats_rdqueue,
			((ats->ats_smsbox.atsrb_state & ATSRB_SMS_SYNC) == 0));

	if(res < 0) {
		return -EINTR;
	} else if(res != 0) {
		return -EINVAL;
	}

	return ats->ats_error;
}



/**
 * Wake up waiting ats devices
 */
static inline void atsld_wakeup(struct atsms *ats, int error)
{
	ats->ats_state = ATSMS_STATE_OK;
	ats->ats_error = error;
	wake_up_interruptible(&ats->ats_queue);
}



/**
 * Wake up reader queues
 */
static inline void atsld_wakeup_sms(struct atsms *ats, int error)
{
	ats->ats_state = ATSMS_STATE_OK;
	ats->ats_error = error;

	/**
	 * If first read wake waiting read else wait one reader
	 */
	if((ats->ats_smsbox.atsrb_state & ATSRB_SMS_FETCHED) == 0)
		wake_up_interruptible(&ats->ats_queue);
	else
		wake_up_interruptible(&ats->ats_rdqueue);
}



/**
 * Add newly received sms index in cmdlist
 *
 * TODO handle diferent memory locations
 * TODO strengthen algorithm for fuzzing
 */
static inline int atsld_sms_add_request(struct atsms *ats, struct atsms_buf *b)
{
	struct atsms_cmd *ac;
	char *idx, *end, *p;
	size_t len;

	/**
	 * Ignore new sms notifications if first fetch has not been done yet
	 */
	if((ats->ats_smsbox.atsrb_state & ATSRB_SMS_FETCHED) == 0)
		return 0;

	end = b->atsb_rxbuf + b->atsb_count - 1;

	idx = memchr(b->atsb_rxbuf, ',', b->atsb_count);

	if(idx >= end)
		return -1;
	++idx;

	len = b->atsb_count  - (idx - b->atsb_rxbuf);

	ac = kmalloc(sizeof(*ac) + len + 1 + STRLEN("+CMGR="), GFP_KERNEL);

	p = ac->atsc_data;

	memcpy(p, "+CMGR=", STRLEN("+CMGR="));
	p += STRLEN("+CMGR=");

	memcpy(p, idx, len);
	p[len] = ';';

	ac->atsc_len = len + 1 + STRLEN("+CMGR=");

	ATSMSLD_DBGMSG("Adding newly received message (%zu) %.*s", len,
			(int)len, idx);

	ats->ats_smsbox.atsrb_state &= ~ATSRB_SMS_SYNC;
	list_add_tail(&ac->atsc_next, &ats->ats_smsbox.atsrb_cmdlist);

	atsld_wakeup_sms(ats, ATSMS_NOERROR);

	return 0;
}



/**
 * Send a message through tty
 */
static inline int atsld_send(struct tty_struct *tty, char const *msg,
		size_t len)
{
	int space;

	space = tty_write_room(tty);
        if (space >= len)
                return tty->ops->write(tty, msg, len);
	set_bit(TTY_DO_WRITE_WAKEUP, &tty->flags);

	return -ENOBUFS;
}



/**
 * Test that AT device is up listenning and in good shape
 */
static inline int atsld_conntest(struct tty_struct *tty, struct atsms *ats)
{
	atsld_send(tty, "AT\r\n", STRLEN("AT\r\n"));

	return atsld_wait(ats);
}



/**
 * Go into sms text mode
 */
static inline int atsld_mode_smstext(struct tty_struct *tty, struct atsms *ats)
{
	int ret;

	atsld_send(tty, "AT+CMGF=1\r\n", STRLEN("AT+CMGF=1\r\n"));
	ats->ats_state = ATSMS_STATE_CHANGE_MODE;

	ret = atsld_wait(ats);
	if(ret == 0)
		ats->ats_mode = ATSMS_MODE_SMSTEXT;

	return ret;
}



/**
 * Check if message from AT device is an error
 *
 * Return 1 if message is an error message 0 otherwise
 * XXX Maybe return the error code
 */
static inline int atsld_rsp_is_error(struct atsms_buf *b)
{
	size_t errlen = STRLEN(ATSMS_ERROR_TOKEN);

	if(b->atsb_count < errlen)
		return 0;
	if(memcmp(b->atsb_rxbuf, ATSMS_ERROR_TOKEN, errlen) != 0)
		return 0;

	return 1;
}



/**
 * Check if message from AT device is a new sms entry
 *
 * Return 1 if message is a new sms entry message 0 otherwise
 */
static inline int atsld_rsp_is_smsentry(struct atsms_buf *b)
{
	size_t len = STRLEN(ATSMS_SMSENTRY_TOKEN);

	if(b->atsb_count < len)
		return 0;
	if(memcmp(b->atsb_rxbuf, ATSMS_SMSENTRY_TOKEN, len) != 0)
		return 0;

	return 1;
}



/**
 * Check if message from AT device has accepted the previsous message
 *
 * Return 1 if message is an ok message 0 otherwise
 */
static inline int atsld_rsp_is_ok(struct atsms_buf *b)
{
	size_t len = STRLEN("OK");

	if(b->atsb_count != len)
		return 0;
	if(memcmp(b->atsb_rxbuf, "OK", len) != 0)
		return 0;

	return 1;
}



/**
 * Check if message from AT device is a sms prompt
 *
 * Return 1 if message is an sms prompt message 0 otherwise
 */
static inline int atsld_rsp_is_smsprompt(struct atsms_buf *b)
{
	size_t len = 1;

	if(b->atsb_count != len)
		return 0;
	if(b->atsb_rxbuf[0] != '>')
		return 0;

	return 1;
}


/**
 * Check if a new message notification arrivied
 *
 * Return 1 if message is new message notification 0 otherwise
 */
static inline int atsld_rsp_is_smsnotif(struct atsms_buf *b)
{
	size_t len = STRLEN("+CMTI: ");

	if(b->atsb_count <= len)
		return 0;
	if(memcmp(b->atsb_rxbuf, "+CMTI: ", len) != 0)
		return 0;

	return 1;
}



/**
 * Receive message and parse it
 */
static void atsld_msg(struct tty_struct *tty, struct atsms *ats)
{
	struct atsms_buf *b = &ats->ats_buf;

	ATSMSLD_DBGMSG("Message received(%lu) : %.*s\n",
			(unsigned long)b->atsb_count,
			(int)b->atsb_count, b->atsb_rxbuf);

	/* Notification messages */
	if(atsld_rsp_is_smsnotif(b))
		atsld_sms_add_request(ats, b);

	/* State dependant responses */
	switch(ats->ats_state) {
	case ATSMS_STATE_OK:
		break;
	case ATSMS_STATE_INIT:
	case ATSMS_STATE_CHANGE_MODE:
	case ATSMS_STATE_SMSG:
	case ATSMS_STATE_PIN:
		if(atsld_rsp_is_ok(b))
			atsld_wakeup(ats, ATSMS_NOERROR);
		else if(atsld_rsp_is_error(b))
			atsld_wakeup(ats, -EINVAL);
		break;
	case ATSMS_STATE_NUM:
		if(atsld_rsp_is_smsprompt(b))
			atsld_wakeup(ats, ATSMS_NOERROR);
		else if(atsld_rsp_is_error(b))
			atsld_wakeup(ats, -EINVAL);
		break;
	case ATSMS_STATE_RMSG:
		if(atsld_rsp_is_ok(b))
			atsld_wakeup(ats, ATSMS_NOERROR);
		else if(atsld_rsp_is_error(b))
			atsld_wakeup(ats, -EINVAL);
		else if(atsld_rsp_is_smsentry(b))
			atsld_sms_new(ats, b);
		else
			atsld_sms_fill(ats, b);
		break;
	default:
		ATSMSLD_ERRMSG("Invalid atsms state %d\n", ats->ats_state);
		break;
	}

	b->atsb_count = 0;
}



/**
 * Parse character
 */
static inline void atsld_receive_char(struct tty_struct *tty,
		struct atsms *ats, unsigned char const c)
{
	struct atsms_buf *b = &ats->ats_buf;

	switch(c) {
	case '\r':
		atsld_msg(tty, ats);
		break;
	case '\n':
		break;
	case '>':
		if(ats->ats_state == ATSMS_STATE_NUM) {
			b->atsb_rxbuf[b->atsb_count++] = c;
			atsld_msg(tty, ats);
		}
		break;
	default :
		b->atsb_rxbuf[b->atsb_count++] = c;
		if(b->atsb_count == ATSMSBUF_LEN)
			atsld_msg(tty, ats);
		break;
	}
}



/**
 * Go fetch for gsm device stored sms
 */
static inline int atsld_stored_sms_fetch(struct tty_struct *tty,
		struct atsms *ats)
{
	ats->ats_state = ATSMS_STATE_RMSG;
	atsld_send(tty, "AT+CMGL=\"ALL\"\r\n", STRLEN("AT+CMGL=\"ALL\"\r\n"));

	return atsld_wait(ats);
}



/**
 * Go fetch for gsm device pending new sms
 */
static inline int atsld_new_sms_sync(struct tty_struct *tty,
		struct atsms *ats)
{
	struct atsms_cmd *ac;

	if((ats->ats_smsbox.atsrb_state & ATSRB_SMS_SYNC) != 0)
		return 0;

	atsld_send(tty, "AT", STRLEN("AT"));

	while(!list_empty(&ats->ats_smsbox.atsrb_cmdlist)) {
		ac = list_first_entry(&ats->ats_smsbox.atsrb_cmdlist,
			struct atsms_cmd, atsc_next);

		atsld_send(tty, ac->atsc_data, ac->atsc_len);

		list_del(&ac->atsc_next);
		kfree(ac);
	}

	ats->ats_state = ATSMS_STATE_RMSG;
	atsld_send(tty, "\r\n", STRLEN("\r\n"));

	return atsld_wait(ats);
}



/**
 * Send a msg by writing AT commands to the tty
 */
static int atsld_sendsms(struct tty_struct *tty, char const *num,
		char const *sms, size_t len)
{
	struct atsms *ats = tty->disc_data;
	int res;
	char ctrlz = 26;

	/* TODO Wait state OK change following to ATSMS_STATE_INIT */

	if(ats->ats_state != ATSMS_STATE_OK) {
		res = atsld_conntest(tty, ats);
		if(res != 0) {
			return res;
		}
	}

	if(ats->ats_mode != ATSMS_MODE_SMSTEXT) {
		res = atsld_mode_smstext(tty, ats);
		if(res != 0) {
			return res;
		}
	}

	/* Send AT+CMGS="+xxxxxxxxxx"\r\n to gsm */
	/* XXX need synchronization ? */
	ats->ats_state = ATSMS_STATE_NUM;
	atsld_send(tty, "AT+CMGS=\"", STRLEN("AT+CMGS=\""));
	atsld_send(tty, num, strlen(num));
	atsld_send(tty, "\"\r\n", STRLEN("\"\r\n"));

	res = atsld_wait(ats);
	if(res < 0)
		return res;


	ats->ats_state = ATSMS_STATE_SMSG;
	atsld_send(tty, sms, len);
	atsld_send(tty, &ctrlz, 1); /* Send Ctrl-Z */

	res = atsld_wait(ats);
	if(res < 0)
		ATSMSLD_DBGMSG("MSG NOT SENT\n");
	else
		ATSMSLD_DBGMSG("MSG SENT\n");
	return res;
}



/**
 * Send a sim pin code by writing AT commands to the tty
 */
static int atsld_sendpin(struct tty_struct *tty, char const *pin, size_t len)
{
	struct atsms *ats = tty->disc_data;
	int res;

	/* TODO Wait state OK change following to ATSMS_STATE_INIT */

	if(ats->ats_state != ATSMS_STATE_OK) {
		res = atsld_conntest(tty, ats);
		if(res != 0) {
			return res;
		}
	}

	if(ats->ats_mode != ATSMS_MODE_SMSTEXT) {
		res = atsld_mode_smstext(tty, ats);
		if(res != 0) {
			return res;
		}
	}

	ATSMSLD_DBGMSG("Sending simpin\n");

	/* Send AT+CPIN="xxxxxxxxxx"\r\n to gsm */
	/* XXX need synchronization ? */
	ats->ats_state = ATSMS_STATE_PIN;
	atsld_send(tty, "AT+CPIN=\"", STRLEN("AT+CPIN=\""));
	atsld_send(tty, pin, len);
	atsld_send(tty, "\"\r\n", STRLEN("\"\r\n"));

	return atsld_wait(ats);
}



/**
 * TTY Openning method for atsms line discipline
 */
static int atsld_open(struct tty_struct *tty)
{
	struct atsms *ats;

	ATSMSLD_DBGMSG("Openning sms tty\n");

	if(tty->disc_data != NULL) {
		return 0;
	}

	ats = atsms_alloc_struct();
	if(ats == NULL)
		return -ENOMEM;

	init_waitqueue_head(&ats->ats_queue);
	init_waitqueue_head(&ats->ats_rdqueue);

	tty->disc_data = ats;

	return 0;
}



/**
 * TTY Closing method for atsms line discipline
 */
static void atsld_close(struct tty_struct *tty)
{
	struct atsms_rcvmsg *m;
	struct atsms_cmd *ac;
	struct atsms *ats;

	ATSMSLD_DBGMSG("Closing sms tty\n");

	if(tty->disc_data == NULL)
		return;

	ats = tty->disc_data;

	while(!list_empty(&ats->ats_smsbox.atsrb_msglist)) {
		m = list_first_entry(&ats->ats_smsbox.atsrb_msglist,
				struct atsms_rcvmsg, atsrm_next);
		list_del(&m->atsrm_next);
		atsms_free_rcvmsg(m);
		ATSMSLD_DBGMSG("Freeing a sms");
	}

	while(!list_empty(&ats->ats_smsbox.atsrb_cmdlist)) {
		ac = list_first_entry(&ats->ats_smsbox.atsrb_cmdlist,
				struct atsms_cmd, atsc_next);
		list_del(&ac->atsc_next);
		kfree(ac);
		ATSMSLD_DBGMSG("Freeing a cmd");
	}

	atsms_free_struct(tty->disc_data);
	tty->disc_data = NULL;
}



/**
 * TTY Receiving buffer method for atsms line discipline
 */
static void atsld_receive_buf(struct tty_struct *tty, unsigned char const *cp,
		char *fp, int count)
{
	int i;

	for(i = 0; i < count; ++i) {
		/* Check parity flag */
		switch(fp[i]) {
		case TTY_NORMAL:
			atsld_receive_char(tty, tty->disc_data, cp[i]);
			break;
		case TTY_OVERRUN:
		case TTY_BREAK:
		case TTY_PARITY:
		case TTY_FRAME:
			ATSMSLD_DBGMSG("Char parity error\n");
			break;
		default:
			ATSMSLD_DBGMSG("Char parity flag unknown\n");
			break;
		}
	}
}



/**
 * TTY Asynchrone write method for atsms line discipline
 */
static void atsld_write_wakeup(struct tty_struct *tty)
{
}



/**
 * TTY reading method for atsms line discipline
 */
static ssize_t atsld_read(struct tty_struct *tty, struct file *file,
		unsigned char __user *buf, size_t nr)
{
	struct atsms *ats = tty->disc_data;
	struct atsms_rcvbox *sbox = &ats->ats_smsbox;
	struct atsms_rcvmsg *m;
	char b[ATS_MSGLEN + 1];
	size_t i;
	ssize_t ret;

	/* TODO Wait state OK */

	/* First sms fetching (fetch all stored sms) */
	if((sbox->atsrb_state & ATSRB_SMS_FETCHED) == 0) {
		ret = atsld_stored_sms_fetch(tty, ats);
		if(ret < 0)
			goto end;
		sbox->atsrb_state |= (ATSRB_SMS_FETCHED | ATSRB_SMS_SYNC);
	}

	/* No more sms in the mailbox, waiting for new ones */
	if(list_empty(&sbox->atsrb_msglist)) {
		ret = atsld_wait_sms(ats);
		ATSMSLD_DBGMSG("Empty sms list wait res : %d\n", (int)ret);
		if(ret < 0)
			goto end;
	}

	/* Fetch all new pending sms */
	ret = atsld_new_sms_sync(tty, ats);
	if(ret < 0)
		goto end;
	sbox->atsrb_state |= ATSRB_SMS_SYNC;

	ret = 0;
	while(!list_empty(&sbox->atsrb_msglist)) {
		m = list_first_entry(&sbox->atsrb_msglist,
				struct atsms_rcvmsg, atsrm_next);

		if(ret + ATSRM_SMSLEN_GET(m) + 1  > nr)
			goto end;

		/* Format message in temporary buffer */
		memcpy(b, m->atsrm_num, sizeof(m->atsrm_num));
		i = sizeof(m->atsrm_num);
		b[i++] = '\n';
		memcpy(b + i, m->atsrm_msg, m->atsrm_len);
		i += m->atsrm_len;
		b[i++] = 26;

		/* Copy message to userspace buffer */
		copy_to_user(buf + ret, b, i);
		ret += i;

		list_del(&m->atsrm_next);
		atsms_free_rcvmsg(m);
		ATSMSLD_DBGMSG("Successfully recv a sms\n");
	}

end:
	return ret;
}



/**
 * This parse the sms the user want to send
 */
static inline ssize_t atsld_write_sms(struct tty_struct *tty,
		unsigned char const *buf, size_t size)
{
	char msg[ATS_MSGLEN];
	char *num, *sms;

	if(size < ATS_NUMLEN + 1) {
		ATSMSLD_ERRMSG("Sms is too short\n");
		return -EFAULT;
	}

	if(size > ATS_MSGLEN) {
		ATSMSLD_ERRMSG("Sms is too long\n");
		return -EFBIG;
	}
	memcpy(msg, buf, size);

	if(msg[ATS_NUMLEN] != '\n') {
		ATSMSLD_ERRMSG("Sms wrong number\n");
		return -EFAULT;
	}

	msg[ATS_NUMLEN] = '\0';
	msg[size] = '\0';
	num = msg;
	sms = msg + ATS_NUMLEN + 1;

	return atsld_sendsms(tty, num, sms, size - ATS_NUMLEN - 1);
}



/**
 * TTY Writting method for atsms line discipline
 */
static ssize_t atsld_write(struct tty_struct *tty, struct file *f,
		unsigned char const *buf, size_t size)
{
	size_t pinlen = STRLEN("PIN=");
	ssize_t ret;

	ATSMSLD_DBGMSG("TTY write method called\n");

	if((size > pinlen) && (memcmp(buf, "PIN=", pinlen) == 0))
		ret = atsld_sendpin(tty, buf + pinlen, size - pinlen);
	else
		ret = atsld_write_sms(tty, buf, size);

	if(ret >= 0)
		ret = size;

	return ret;
}



/**
 * TTY flush buffer method for atsms line discipline
 */
static void atsld_flush_buffer(struct tty_struct *tty)
{
}



/**
 * TTY char in buffer method for atsms line discipline
 */
static ssize_t atsld_chars_in_buffer(struct tty_struct *tty)
{
	return 0;
}



/* Line discipline for tty */
struct tty_ldisc_ops tty_ldisc_sms = {
	.owner           = THIS_MODULE,
	.magic           = TTY_LDISC_MAGIC,
	.name            = "n_atsms",
	.open            = atsld_open,
	.close           = atsld_close,
	.flush_buffer    = atsld_flush_buffer,
	.chars_in_buffer = atsld_chars_in_buffer,
	.read		 = atsld_read,
	.write           = atsld_write,
	.receive_buf     = atsld_receive_buf,
	.write_wakeup    = atsld_write_wakeup
};




int atsld_init(void)
{
	int error;

	ATSMSLD_DBGMSG("n_atsms(%u): Line discipline module init\n", N_ATSMS);

	error = tty_register_ldisc(N_ATSMS, &tty_ldisc_sms);
	if(error != 0) {
		ATSMSLD_ERRMSG("n_atsms: can't register line discipline (err = %d)\n",
				error);
		return error;
	}

	return 0;
}
EXPORT_SYMBOL(atsld_init);




void atsld_exit(void)
{
	int error;
	ATSMSLD_DBGMSG("n_atsms(%u): Line discipline module exit\n", N_ATSMS);

	error = tty_unregister_ldisc(N_ATSMS);
	if(error != 0) {
		ATSMSLD_ERRMSG("n_atsms: can't unregister line discipline (err = %d)\n",
				error);
	}

}
EXPORT_SYMBOL(atsld_exit);


