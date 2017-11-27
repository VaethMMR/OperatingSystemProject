/*
 *  File:  libuser.c
 *
 *  Description:  This file contains the interface declarations
 *                to the OS kernel support package.
 *
 */

#include <phase1.h>
#include <phase2.h>
#include <libuser.h>
#include <usyscall.h>
#include <usloss.h>

#define CHECKMODE {    \
if (USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE) { \
    USLOSS_Console("Trying to invoke syscall from kernel\n"); \
    USLOSS_Halt(1);  \
    }  \
}

/*
 *  Routine:  Spawn
 *
 *  Description: This is the call entry to fork a new user process.
 *
 *  Arguments:    char *name    -- new process's name
 *                PFV func      -- pointer to the function to fork
 *                void *arg     -- argument to function
 *                int stacksize -- amount of stack to be allocated
 *                int priority  -- priority of forked process
 *                int  *pid     -- pointer to output value
 *                (output value: process id of the forked process)
 *
 *  Return Value: 0 means success, -1 means error occurs
 *
 */
int Spawn(char *name, int (*func)(char *), char *arg, int stack_size, 
    int priority, int *pid)   
{
    systemArgs sysArg;
    
    CHECKMODE;
    sysArg.number = SYS_SPAWN;
    sysArg.arg1 = (void *) func;
    sysArg.arg2 = arg;
    sysArg.arg3 = (void *) (long) stack_size;
    sysArg.arg4 = (void *) (long) priority;
    sysArg.arg5 = name;

    USLOSS_Syscall(&sysArg);

    *pid = (long) sysArg.arg1;
    return (long) sysArg.arg4;
} /* end of Spawn */


/*
 *  Routine:  Wait
 *
 *  Description: This is the call entry to wait for a child completion
 *
 *  Arguments:    int *pid -- pointer to output value 1
 *                (output value 1: process id of the completing child)
 *                int *status -- pointer to output value 2
 *                (output value 2: status of the completing child)
 *
 *  Return Value: 0 means success, -1 means error occurs
 *
 */
int Wait(int *pid, int *status)
{
    systemArgs sysArg;
    
    CHECKMODE;
    sysArg.number = SYS_WAIT;

    USLOSS_Syscall(&sysArg);

    *pid = (long) sysArg.arg1;
    *status = (long) sysArg.arg2;
    return (long) sysArg.arg4;
    
} /* end of Wait */


/*
 *  Routine:  Terminate
 *
 *  Description: This is the call entry to terminate 
 *               the invoking process and its children
 *
 *  Arguments:   int status -- the commpletion status of the process
 *
 *  Return Value: 0 means success, -1 means error occurs
 *
 */
void Terminate(int status)
{
    systemArgs sysArg;
    
    CHECKMODE;
    sysArg.number = SYS_TERMINATE;
    
    sysArg.arg1 = (void *) (long) status;
    USLOSS_Syscall(&sysArg);
    
    
} /* end of Terminate */

/*
 *  Routine:  SemCreate
 *
 *  Description: Create a semaphore.
 *
 *  Arguments:
 *              int value       -- the initial count to give the semaphore
 *              int *semaphore  -- the address to store the semaphore id
 *
 *  return: 0 if semaphore created, -1 if error occurs
 *  side effect semaphore will store the semaphore id
 */
int SemCreate(int value, int *semaphore)
{
    systemArgs args;
    
    CHECKMODE;
    args.number = SYS_SEMCREATE;
    args.arg1 = (void *) (long) value;
    USLOSS_Syscall(&args);
    
    *semaphore = (long) args.arg1;
    return (long) args.arg4;
} /* end of SemCreate */

/*
 *  Routine:  SemP
 *
 *  Description: "P" a semaphore.
 *
 *  Arguments: int semaphore    -- the semaphore id to P
 *
 *  return: 0 semaphore P'd
 *         -1 invalid arguments
 */
int SemP(int semaphore)
{
    systemArgs args;
    
    CHECKMODE;
    args.number = SYS_SEMP;
    args.arg1 = (void *) (long) semaphore;
    USLOSS_Syscall(&args);
    return (long) args.arg4;
} /* end of SemP */


/*
 *  Routine:  SemV
 *
 *  Description: "V" a semaphore.
 *
 *  Arguments: int semaphore    -- the semaphore id to V
 *
 *  return: 0 semaphore V'd
 *          -1 invalid arguments
 */
int SemV(int semaphore)
{
    systemArgs args;
    
    CHECKMODE;
    args.number = SYS_SEMV;
    args.arg1 = (void *) (long) semaphore;
    
    USLOSS_Syscall(&args);
    return (long) args.arg4;
} /* end of SemV */


/*
 *  Routine:  SemFree
 *
 *  Description: Free a semaphore.
 *
 *  Arguments:  int semaphore   -- the semaphore id to free
 *
 *  Return: 0 if freed with no blocked processes
 *          1 if freed with blocked processes
 *         -1 if invalid arguments
 */
int SemFree(int semaphore)
{
    systemArgs args;
    
    CHECKMODE;
    args.number = SYS_SEMFREE;
    args.arg1 = (void *) (long) semaphore;
    
    USLOSS_Syscall(&args);
    return (long) args.arg4;
} /* end of SemFree */


/*
 *  Routine:  GetTimeofDay
 *
 *  Description: This is the call entry point for getting the time of day.
 *
 *  Arguments:  int *tod    -- the address for the time of day
 *
 *  Return: none
 */
void GetTimeofDay(int *tod)                           
{
    systemArgs args;
    
    CHECKMODE;
    args.number = SYS_GETTIMEOFDAY;
    USLOSS_Syscall(&args);

    *tod = (long) args.arg1;
} /* end of GetTimeofDay */

/*
 *  Routine:  CPUTime
 *
 *  Description: This is the call entry point for the process' CPU time.
 *
 *  Arguments: int *cpu -- the address for the cpu time
 *
 *  return none
 */
void CPUTime(int *cpu)                           
{
    systemArgs args;
    
    CHECKMODE;
    args.number = SYS_CPUTIME;
    USLOSS_Syscall(&args);
    *cpu = (long) args.arg1;
} /* end of CPUTime */


/*
 *  Routine:  GetPID
 *
 *  Description: This is the call entry point for the process' PID.
 *
 *  Arguments: int* pid     -- the address for the pid
 *
 * returns none
 */
void GetPID(int *pid)                           
{
    systemArgs args;
    
    CHECKMODE;
    args.number = SYS_GETPID;
    USLOSS_Syscall(&args);
    *pid = (long) args.arg1;
} /* end of GetPID */

/* end libuser.c */
