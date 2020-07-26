#ifndef __MASTERMIND_H
#define __MASTERMIND_H

#include <linux/ioctl.h> /* needed for the _IOW etc stuff used later */


#define MASTERMIND_IOC_MAGIC  'k'
#define MASTERMIND_MMIND_REMAINING	_IO(MASTERMIND_IOC_MAGIC,  0)
#define MASTERMIND_MMIND_ENDGAME	_IO(MASTERMIND_IOC_MAGIC,   1)
#define MASTERMIND_MMIND_NEWGAME	_IOW(MASTERMIND_IOC_MAGIC,  2, char *)
#define MASTERMIND_IOC_MAXNR 2

#endif
