#ifndef _N_ATSMS_H_
#define _N_ATSMS_H_

#define N_ATSMS 25

#ifdef ATSMSLD_DBG
#define ATSMSLD_DBGMSG(...) printk(KERN_ALERT __VA_ARGS__)
#define ATSMSLD_ERRMSG(...) printk(KERN_ERR __VA_ARGS__)
#else
#define ATSMSLD_DBGMSG(...)
#define ATSMSLD_ERRMSG(...)
#endif


int atsld_init(void);
void atsld_exit(void);

#endif
