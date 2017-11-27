
#include "usloss.h"

#define DEBUG 0
extern int debugflag;

extern void checkKernelMode(char *);

void
p1_fork(int pid)
{
    checkKernelMode("p1_fork");
    if (DEBUG && debugflag)
        USLOSS_Console("p1_fork() called: pid = %d\n", pid);
} /* p1_fork */

void
p1_switch(int old, int new)
{
    checkKernelMode("p1_switch");
    if (DEBUG && debugflag)
        USLOSS_Console("p1_switch() called: old = %d, new = %d\n", old, new);
} /* p1_switch */

void
p1_quit(int pid)
{
    checkKernelMode("p1_quit");
    if (DEBUG && debugflag)
        USLOSS_Console("p1_quit() called: pid = %d\n", pid);
} /* p1_quit */
