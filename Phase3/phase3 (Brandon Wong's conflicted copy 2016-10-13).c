#include <usloss.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include "libuser.h"
#include <usyscall.h>
#include "string.h"

// ====================PROTOTYPES=======================
extern int start3(char *);
int start2(char *);
void nullsys3(systemArgs*);

void spawn(systemArgs *);
void waitt(systemArgs *);
void terminate(systemArgs *);
void semCreate(systemArgs *);
void semP(systemArgs *);
void semV(systemArgs *);
void semFree(systemArgs *);
void getTimeofDay(systemArgs *);
void cpuTime(systemArgs *);
void getPID(systemArgs *);

int spawnReal(char *, int (*)(char *), char *, int, int);
int waitReal(int *);
void terminateReal(int);
int semCreateReal(int);
void semPReal(int);
void semVReal(int);
int semFreeReal(int);
int getTimeofDayReal(void);
int cpuTimeReal(void);
int getPIDReal(void);

int spawnLaunch(char *);

void addSpawn(procPtr);
void removeSpawn(procPtr);
void addToSemList(semPtr, procPtr);
void removeFromSemList(semPtr);

void zapFamily(procPtr);
void zapFamilies(procPtr);

void changeMode();

void clearProc(int);
void clearSem(int);
void clearSyscall(int);

// ====================GLOBALS=======================
procStruct ProcTable[MAXPROC];
semaphore SemTable[MAXSEMS];
extern void (*systemCallVec[MAXSYSCALLS])(systemArgs*);
procPtr Current;
int start3Pid;

int semCount = 0;

int debugSpawn = 0;
int debugWait = 0;
int debugTerminate = 0;
int debugSemCreate = 0;
int debugSemP = 0;
int debugSemV = 0;
int debugSemFree = 0;
int debugGetTimeOfDay = 0;
int debugCPUTime = 0;
int debugGetPID = 0;

// ====================FUNCTIONS=======================
int start2(char *arg)
{
    int i;
    int pid;
    int status;
    
    /*
     * Check kernel mode here.
     */
    CHECKMODE;

    /*
     * Data structure initialization as needed...
     */
    for(i = 0; i < MAXPROC; i++){
        clearProc(i);
    }

    for(i = 0; i < MAXSEMS; i++){
        clearSem(i);
    }

    for(i = 0; i < MAXSYSCALLS; i++){
        clearSyscall(i);
    }
    systemCallVec[SYS_SPAWN] = spawn;
    systemCallVec[SYS_WAIT] = waitt;
    systemCallVec[SYS_TERMINATE] = terminate;
    systemCallVec[SYS_SEMCREATE] = semCreate;
    systemCallVec[SYS_SEMP] = semP;
    systemCallVec[SYS_SEMV] = semV;
    systemCallVec[SYS_SEMFREE] = semFree;
    systemCallVec[SYS_GETTIMEOFDAY] = getTimeofDay;
    systemCallVec[SYS_CPUTIME] = cpuTime;
    systemCallVec[SYS_GETPID] = getPID;

    /*
     * Create first user-level process and wait for it to finish.
     * These are lower-case because they are not system calls;
     * system calls cannot be invoked from kernel mode.
     * Assumes kernel-mode versions of the system calls
     * with lower-case names.  I.e., Spawn is the user-mode function
     * called by the test cases; spawn is the kernel-mode function that
     * is called by the syscallHandler; spawnReal is the function that
     * contains the implementation and is called by spawn.
     *
     
     * Spawn() is in libuser.c.  It invokes USLOSS_Syscall()
     * The system call handler calls a function named spawn() -- note lower
     * case -- that extracts the arguments from the sysargs pointer, and
     * checks them for possible errors.  This function then calls spawnReal().
     *
     
     * Here, we only call spawnReal(), since we are already in kernel mode.
     *
     
     * spawnReal() will create the process by using a call to fork1 to
     * create a process executing the code in spawnLaunch().  spawnReal()
     * and spawnLaunch() then coordinate the completion of the phase 3
     * process table entries needed for the new process.  spawnReal() will
     * return to the original caller of Spawn, while spawnLaunch() will
     * begin executing the function passed to Spawn. spawnLaunch() will
     * need to switch to user-mode before allowing user code to execute.
     * spawnReal() will return to spawn(), which will put the return
     * values back into the sysargs pointer, switch to user-mode, and 
     * return to the user code that called Spawn.
     */
    pid = spawnReal("start3", start3, NULL, USLOSS_MIN_STACK, 3);
    start3Pid = pid;

    /* Call the waitReal version of your wait code here.
     * You call waitReal (rather than Wait) because start2 is running
     * in kernel (not user) mode.
     */
    pid = waitReal(&status);
    
    return 0;

} /* start2 */

void spawn(systemArgs *args)
{
    args->arg1 = (void *) (long) spawnReal(args->arg5, args->arg1, args->arg2, (int) args->arg3, (int) args->arg4);
    changeMode();
} /* end of Spawn */

/*
 *  Routine:  SpawnReal
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

int spawnReal(char *name, int (*func)(char *), char *arg, int stack_size,
              int priority){
    int pid;
    int res;
    
    //find pid of new spawn proc
    for(pid = getpid() + 1; pid < MAXSPAWNS || ProcTable[pid].status != UNUSED; pid++);
    
    if(pid == MAXPROC){
        USLOSS_Console("Table full\n");
        USLOSS_Halt(1);
    }

    // Create spawn entry
    procPtr new = &ProcTable[pid % MAXPROC];
    new->mboxID = MboxCreate(0, 0);
    new->func = func;
    new->status = USED;
    new->pid = pid;
    new->parentPtr = &ProcTable[getpid() % MAXPROC];
    strcpy(new->arg, arg);
    
    //add spawn proc to children of current proc
    addSpawn(new);
    
    //fork spawn
    res = fork1(name, spawnLaunch, arg, stack_size, priority);
    return res;
}

int spawnLaunch(char *arg)
{
    int result;
    procPtr cur = &ProcTable[getpid() % MAXPROC];
    if(DEBUG3 && debugSpawn)
        USLOSS_Console("spawnLaunch(): entering...\n");
    if(DEBUG3 && debugSpawn)
        USLOSS_Console("spawnLaunch(): pid was %d\n", getpid());
    cur->pid = getpid();
    changeMode();
    result = cur->func(cur->arg);
    if(DEBUG3 && debugSpawn)
        USLOSS_Console("spawnLaunch(): process ended, terminating\n");

    // get back in kernel mode to terminate process
    // make a syscall to Terminate
    Terminate(result);
    return 0; //so compiler doesn't complain
}

/*
 *  Routine:  Wait
 *
 *  Description:  extract arguments for make a process wait
 *
 *  Arguments:    int *pid -- pointer to output value 1
 *                (output value 1: process id of the completing child)
 *                int *status -- pointer to output value 2
 *                (output value 2: status of the completing child)
 *
 *  Return Value: 0 means success, -1 means error occurs
 *
 */
void waitt(systemArgs *args)
{
    if(DEBUG3 && debugWait)
        USLOSS_Console("wait: in wait\n");
    int status, pid;
    int result = 0;
    pid = waitReal(&status);
    if(pid < 0)
        result = -1;
    
    if(DEBUG3 && debugWait)
        USLOSS_Console("wait: done waiting pid %d status %d result %d\n", pid, status, result);
    
    args->arg1 = (void *) (long) pid;
    args->arg2 = (void *) (long) status;
    args->arg4 = (void *) (long) result;
    changeMode();
    
} /* end of Wait */

/*
 *  Routine:  WaitReal
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

int waitReal(int *status){
    if(DEBUG3 && debugWait)
        USLOSS_Console("waitReal: joining %d\n", getpid());
    return join(status);
}

/*
 *  Routine:  Terminate
 *
 *  Description: extract arguments for terminating a process
 *
 *  Arguments:   int status -- the commpletion status of the process
 *
 *  Return Value: 0 means success, -1 means error occurs
 *
 */

void terminate(systemArgs *args)
{
    if(DEBUG3 && debugTerminate)
        USLOSS_Console("terminate: terminating %d\n", getpid());
    terminateReal((int) args->arg1);
    changeMode();
} /* end of Terminate */

/*
 *  Routine:  TerminateReal
 *
 *  Description: This is the call entry to terminate
 *               the invoking process and its children
 *
 *  Arguments:   int status -- the commpletion status of the process
 *
 *  Return Value: 0 means success, -1 means error occurs
 *
 */

void terminateReal(int status){
    procPtr Current = &ProcTable[getpid() % MAXPROC];
    
    if(DEBUG3 && debugTerminate)
        USLOSS_Console("terminateReal: in terminateReal\n");
    
    // zap all children processes, remove from spawn list
    // if child has children, recursively call terminateReal to terminate descendants
    // else iteratively zap children
    zapFamily(Current->childProc);
    
    // if start3 is terminating, shutdown
    if(getpid() == start3Pid){
        if(DEBUG3 && debugTerminate){
            USLOSS_Console("Start 3 terminating. Halting...\n");
        }
        USLOSS_Halt(0);
    }

    // if current process is clearing, dereference parentPtr
    Current->parentPtr->childProc = Current->nextSiblingProc;
    clearProc(Current->pid);
    
    // current running proc can not zap itself, must quit
    quit(status);
}

/*
 *  Routine:  SemCreate
 *
 *  Description: extract arguments for creating a semaphore
 *
 *  Arguments:
 *
 */

void semCreate(systemArgs *args){
    int result;
    result = semCreateReal((int) args->arg1);
    
    if(result != -1){
        args->arg1 = (void *) (long) result;
        args->arg4 = 0;
    }
    
    changeMode();
} /* end of SemCreate */

/*
 *  Routine:  SemCreateReal
 *
 *  Description: Create a semaphore.
 *
 *  Arguments:
 *
 */

int semCreateReal(int count){
    int i, semID;
    
    // cheksums: semaphore must be non-negative and sem table must have space for it
    if(semCount >= MAXSEMS){
        if(DEBUG3 && debugSemCreate){
            USLOSS_Console("semCreateReal: sem table full\n");
        }
        return -1;
    }
    
    if(count < 0){
        if(DEBUG3 && debugSemCreate){
            USLOSS_Console("semCreateReal: %d is negative, must be non-negative\n", count);
        }
        return -1;
    }
    
    // find semID
    for(i = 0; SemTable[i % MAXSEMS].count != UNUSED; i++);
    semID = i;
    
    //create semaphore
    SemTable[i].count = count;
    SemTable[i].semID = semID;
    
    // increment sem counter
    semCount++;
    
    return semID;
}

/*
 *  Routine:  SemP
 *
 *  Description: extract arguments for invoking "P" on a semaphore
 *
 *  Arguments:
 *
 */

void semP(systemArgs *args)
{
    int semID = (int) args->arg1;
    
    if(semID < 0 || semID > MAXSEMS){
        if(DEBUG3 && debugSemP){
            USLOSS_Console("semaphore %d out of range\n", semID);
            args->arg4 = (void *) (long) -1;
        }
        else if(SemTable[semID % MAXSEMS].count == UNUSED){
            USLOSS_Console("semaphore %d not in use\n", semID);
            args->arg4 = (void *) (long) -1;
        }
        else{
            semPReal(semID);
            args->arg4 = 0;
        }
    }
} /* end of SemP */

/*
 *  Routine:  SemPReal
 *
 *  Description: "P" a semaphore.
 *
 *  Arguments:
 *
 */

void semPReal(int semID){
    semPtr sem = &SemTable[semID % MAXSEMS];
    procPtr Current = &ProcTable[getpid() % MAXPROC];
    if(sem->count == 0){
        if(DEBUG3 && debugSemP){
            USLOSS_Console("semaphor %d at 0, will block\n", semID);
        }
        addToSemList(sem, Current);
        if(Current->status == ZAPPING){
            if(DEBUG3 && debugSemP){
                USLOSS_Console("semPReal: process %d was zapped\n", Current->pid);
            }
            terminateReal(0);
        }
    }
    
    sem->count--;
}

/*
 *  Routine:  SemV
 *
 *  Description: extract arguments for invoking "V" on a semaphore
 *
 *  Arguments:
 *
 */

void semV(systemArgs *args)
{
    int semID = (int) args->arg1;
    
    if(semID < 0 || semID > MAXSEMS){
        if(DEBUG3 && debugSemP){
            USLOSS_Console("semaphore %d out of range\n", semID);
            args->arg4 = (void *) (long) -1;
        }
        else if(SemTable[semID % MAXSEMS].count == UNUSED){
            USLOSS_Console("semaphore %d not in use\n", semID);
            args->arg4 = (void *) (long) -1;
        }
        else{
            semVReal(semID);
            args->arg4 = 0;
        }
    }

    
} /* end of SemV */

/*
 *  Routine:  SemVReal
 *
 *  Description: "V" a semaphore.
 *
 *  Arguments:
 *
 */

void semVReal(int semID){

    semPtr sem = &SemTable[semID % MAXSEMS];
    
    //increase sem counter
    sem->count++;

    //unblock next sem blocked proc
    removeFromSemList(sem);
}

/*
 *  Routine:  SemFree
 *
 *  Description: extract arguments for freeing a semaphore
 *
 *  Arguments:
 *
 */

void semFree(systemArgs *args)
{
    int semID = (int) args->arg1;
    
    if(semID < 0 || semID > MAXSEMS){
        if(DEBUG3 && debugSemP){
            USLOSS_Console("semaphore %d out of range\n", semID);
            args->arg4 = (void *) (long) -1;
        }
        else if(SemTable[semID % MAXSEMS].count == UNUSED){
            USLOSS_Console("semaphore %d not in use\n", semID);
            args->arg4 = (void *) (long) -1;
        }
        else{
            semFreeReal(semID);
            args->arg4 = 0;
        }
    }
} /* end of SemFree */

/*
 *  Routine:  SemFreeReal
 *
 *  Description: Free a semaphore.
 *
 *  Arguments:
 *
 */

int semFreeReal(int semID){
    semPtr sem = &SemTable[semID % MAXSEMS];
    
    // terminate any blocked process
    zapFamilies(sem->blockedPtr);
    
    // clear the semaphore entry
    clearSem(semID);
    return 0;
}

/*
 *  Routine:  GetTimeofDay
 *
 *  Description: extract arguments for getting the time of day
 *
 *  Arguments:
 *
 */

void getTimeofDay(systemArgs *args)
{
    args->arg1 = (void *) (long) getTimeofDayReal();
} /* end of GetTimeofDay */


/*
 *  Routine:  GetTimeofDayReal
 *
 *  Description: This is the call entry point for getting the time of day.
 *
 *  Arguments:
 *
 */
int getTimeofDayReal(){
    return USLOSS_Clock();
}

/*
 *  Routine:  CPUTime
 *
 *  Description: extract arguments for getting the process' CPU time
 *
 *  Arguments:
 *
 */

void cpuTime(systemArgs *args)
{
    args->arg1 = (void *) (long) cpuTimeReal();
} /* end of CPUTime */

/*
 *  Routine:  CPUTimeReal
 *
 *  Description: This is the call entry point for the process' CPU time.
 *
 *  Arguments:
 *
 */

int cpuTimeReal(){
    return USLOSS_Clock() - readCurStartTime();
}

/*
 *  Routine:  GetPID
 *
 *  Description: extract arguments for getting the process' PID
 *
 *  Arguments:
 *
 */

void getPID(systemArgs *args)
{
    args->arg1 = (void *) (long) getPIDReal();
} /* end of GetPID */

/*
 *  Routine:  GetPIDReal
 *
 *  Description: This is the call entry point for the process' PID.
 *
 *  Arguments:
 *
 */

int getPIDReal(){
    return getpid();
}

void zapFamily(procPtr root){
    procPtr cur;
    if(root == NULL){
        return;
    }
    // go through siblings
    for(cur = root; cur != NULL; cur = root){
        //if sibling has child zap child's family
        if(cur->childProc != NULL){
            zapFamily(cur->childProc);
        }

        //schedule for zap
        cur->status = ZAPPING;
        
        //if process is blocked on semaphore remove it from semaphore
        if(cur->sem != NULL)
            removeFromSemList(cur->sem);
        
        // update parent to point to next child
        root->parentPtr->childProc = root->nextSiblingProc;
        
        // zap all processes
        zap(cur->pid);
        
        clearProc(cur->pid % MAXPROC);
    }
}

void zapFamilies(proctPtr ptr){
	
}

void changeMode(){
    USLOSS_PsrSet(USLOSS_PsrGet() - USLOSS_PSR_CURRENT_MODE);
}

void addSpawn(procPtr new){
    procPtr cur = ProcTable[getpid() % MAXPROC].childProc;
    if(cur == NULL){
        ProcTable[getpid() % MAXPROC].childProc = new;
    }
    else{
        for(; cur->nextSiblingProc != NULL; cur = cur->nextSiblingProc);
        cur->nextSiblingProc = new;
    }
}

void removeSpawn(procPtr spawn){
    procPtr cur = ProcTable[getpid() % MAXPROC].childProc;
    procPtr prev;
    if(cur != NULL){
        if(cur->pid == spawn->pid)
            ProcTable[getpid() % MAXPROC].childProc = cur->nextSiblingProc;
        else{
            for(; cur->nextSiblingProc != NULL; cur = cur->nextSiblingProc)
                if(cur->nextSiblingProc->pid == spawn->pid){
                    prev = cur;
                    cur = cur->nextSiblingProc;
                    prev->nextSiblingProc = cur->nextSiblingProc;
                    cur->nextSiblingProc = NULL;
                }
        }
    }
}

void addToSemList(semPtr sem, procPtr proc){
    procPtr cur;
    if(sem->blockedPtr == NULL){
        sem->blockedPtr = proc;
    }
    else{
        for(cur = sem->blockedPtr; cur->nextBlockedPtr != NULL; cur = cur->nextBlockedPtr);
        cur->nextBlockedPtr = proc;
    }
    cur->sem = sem;
    MboxSend(proc->mboxID, NULL, 0);
}

void removeFromSemList(semPtr sem){
    procPtr cur = sem->blockedPtr;
    if(cur != NULL){
        sem->blockedPtr = cur->nextBlockedPtr;
        cur->nextBlockedPtr = NULL;
        cur->sem = NULL;
        MboxReceive(cur->mboxID, NULL, 0);
    }
}


/* ------------------------------------------------------------------------
 Name - nullsys3
 Purpose - prints error message and halt USLOSS
 Parameters - one
 Returns - one to indicate normal quit.
 Side Effects - lots since it initializes the phase2 data structures.
 ----------------------------------------------------------------------- */
void nullsys3(systemArgs *args)
{
    USLOSS_Console("nullsys(): Invalid syscall %d. Halting...\n", args->number);
    USLOSS_Halt(1);
} /* nullsys */

/* ------------------------------------------------------------------------
 Name - clearProc
 Purpose - clear process in the process table
 Parameters - the index of the process in the process table
 Returns - none
 Side Effects - none
 ------------------------------------------------------------------------ */
void clearProc(int idx){
    ProcTable[idx].pid = -1;
    ProcTable[idx].status = UNUSED;
    ProcTable[idx].func = NULL;
    MboxRelease(ProcTable[idx].mboxID);
    ProcTable[idx].childProc = NULL;
    ProcTable[idx].sem = NULL;
    ProcTable[idx].nextSiblingProc = NULL;
    ProcTable[idx].parentPtr = NULL;
    ProcTable[idx].nextBlockedPtr = NULL;
}

/* ------------------------------------------------------------------------
 Name - clearProc
 Purpose - clear process in the process table
 Parameters - the index of the process in the process table
 Returns - none
 Side Effects - none
 ------------------------------------------------------------------------ */
void clearSem(int idx){
    SemTable[idx].count = -1;
    SemTable[idx].semID = -1;
    SemTable[idx].blockedPtr = NULL;
}

/* ------------------------------------------------------------------------
 Name - clearProc
 Purpose - clear process in the process table
 Parameters - the index of the process in the process table
 Returns - none
 Side Effects - none
 ------------------------------------------------------------------------ */
void clearSyscall(int idx){
    systemCallVec[idx] = nullsys3;
}

