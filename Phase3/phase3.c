#include <usloss.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include "libuser.h"
#include <usyscall.h>
#include <sems.h>
#include <stdlib.h>

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
void removeSpawn(procPtr, procPtr);
void addToSemList(semPtr, procPtr);
void removeFromSemList(semPtr);

void changeMode();
void checkKernel(char *);

void printFamily(procPtr);
void printSemList(semPtr);

void clearProc(int);
void clearSem(int);
void clearSyscall(int);

// ====================GLOBALS=======================
procStruct ProcTable[MAXPROC];
semaphore SemTable[MAXSEMS];
extern void (*systemCallVec[MAXSYSCALLS])(systemArgs*);

int spawnCount = 0;
int semCount = 0;

int mutexSem;

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
int debugKernel = 0;

// ====================FUNCTIONS=======================
int start2(char *arg)
{
    int i;
    int status;
    
    /*
     * Check kernel mode here.
     */
    checkKernel("start2");
    
    // mutex for semaphore functions
    mutexSem = MboxCreate(1, 0);

    /*
     * Data structure initialization as needed...
     */
    for(i = 0; i < MAXPROC; i++){
        clearProc(i);
        ProcTable[i].mboxID = MboxCreate(0, 0);
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
    spawnReal("start3", start3, NULL, USLOSS_MIN_STACK, 3);

    /* Call the waitReal version of your wait code here.
     * You call waitReal (rather than Wait) because start2 is running
     * in kernel (not user) mode.
     */
    waitReal(&status);
    return 0;

} /* start2 */

/*
 *  Routine:  spawn
 *
 *  Description: extract arguments to make a process spawn
 *
 *  Arguments:    sytemArgs * args     -- the systemArgs struct containing spawn arguments
 *
 *  Return Value: None
 *  side effects: sys arg1 will store return status of spawning a process
 *
 */

void spawn(systemArgs *args)
{
    checkKernel("spawn");
    args->arg1 = (void *) (long) spawnReal(args->arg5, args->arg1, args->arg2, (long) args->arg3, (long) args->arg4);
    changeMode();
} /* end of Spawn */

/*
 *  Routine:  SpawnReal
 *
 *  Description: spawn a new process for user mode functions
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
    
    checkKernel("spawnReal");
    if(spawnCount >= MAXSPAWNS){
        if(DEBUG3 && debugSpawn){
            USLOSS_Console("spawnReal: process table is full\n");
        }
        return -1;
    }
    
    // get the pid of the spawn proc
    pid = fork1(name, spawnLaunch, arg, stack_size, priority);
    
    //increment spawn count
    spawnCount++;
    
    // Create spawn entry
    procPtr new = &ProcTable[pid % MAXPROC];

    new->pid = pid;
    
    new->func = func;
    new->status = USED;
    new->parentPtr = &ProcTable[getpid() % MAXPROC];
    new->name = name;
    new->arg = arg;

    if(DEBUG3 && debugSpawn){
        USLOSS_Console("spawnReal: populating proc %d\n", new->pid);
    }
   
    //add spawn proc to children of current proc
    addSpawn(new);
    
    // send to mailbox in case blocked
    MboxCondSend(new->mboxID, NULL, 0);
    return pid;
}

int spawnLaunch(char *arg)
{
    int result;
    checkKernel("spawnLaunch");
    procPtr cur = &ProcTable[getpid() % MAXPROC];
    
    // higher priority spawns are uninitialized, block for initialization
    if(cur->status == UNUSED){
        MboxReceive(cur->mboxID, NULL, 0);
    }
    
    if(isZapped()){
        terminateReal(1);
    }
    if(DEBUG3 && debugSpawn)
        USLOSS_Console("spawnLaunch(): entering...\n");
    if(DEBUG3 && debugSpawn)
        USLOSS_Console("spawnLaunch(): pid %d\n", getpid());
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
 *  Routine:  waitt
 *
 *  Description:  extract arguments for make a process wait
 *
 *  Arguments:    systemArgs* args  -- the systemArgs struct containing wait arguments
 *
 *  Return Value: nonw
 *  side effects: arg1 will store the pid of the waiting process
 *                arg2 will store the status passed into wait
 *                arg4 will store the 0 if succesful and -1 if error
 *
 */
void waitt(systemArgs *args)
{
    checkKernel("wait");
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
 *  Description: Make a parent wait on its child. "wait" is simply blocking the process
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
    int result;
    checkKernel("waitReal");
    if(DEBUG3 && debugWait)
        USLOSS_Console("waitReal: joining %d\n", getpid());
    result = join(status);
    return result;
}

/*
 *  Routine:  Terminate
 *
 *  Description: extract arguments for terminating a process
 *
 *  Arguments:   systemArgs* args   -- the systemArgs struct containing arguments for terminate
 *
 *  Return Value: none
 *  side effect: none
 *
 */

void terminate(systemArgs *args)
{
    checkKernel("terminate");
    if(DEBUG3 && debugTerminate)
        USLOSS_Console("terminate: terminating %d\n", getpid());
    terminateReal((long) args->arg1);
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
    procPtr cur, prev;
 
    checkKernel("terminateReal");
    if(DEBUG3 && debugTerminate)
        USLOSS_Console("terminateReal: in terminateReal\n");
    
    // zap all children processes, remove from spawn list
    // if child has children, recursively call terminateReal to terminate descendants
    // else iteratively zap children

    if(DEBUG3 && debugTerminate){
        if(Current->childProc == NULL){
            USLOSS_Console("terminateReal: proc %d no children\n", Current->pid);
        }
        printFamily(Current);
    }
    
    if(Current->childProc != NULL){
        // go through siblings
        for(cur = Current->childProc; cur != NULL;){
            
            if(DEBUG3 && (debugTerminate || debugSemFree)){
                USLOSS_Console("Proc %d scheduled for zapping\n", cur->pid);
            }
            
            //ptr to zapping proc
            prev = cur;
            
            //update cur
            cur = cur->nextSiblingProc;
            
            // zap proc
            zap(prev->pid);
        }

    }

    // terminate process, restructure any family procs
    //remove the process from the parent's spawn tree
    // update parent to point to next child
    removeSpawn(Current->parentPtr, Current);
    
    // clear the process entry
    clearProc(Current->pid);
    
    // current running proc can not zap itself, must quit
    spawnCount--;
    
    // quit the process
    quit(status);
}

/*
 *  Routine:  SemCreate
 *
 *  Description: extract arguments for creating a semaphore
 *
 *  Arguments: systemArgs* args -- systemArgs struct containing arguments for
 *                                  creating a semaphore
 *
 *  Return: none
 *  side effects: arg1 stores the result of creating a semaphore
 *                arg4 stores 0 if successful, -1 if not
 */

void semCreate(systemArgs *args){
    int result;
    checkKernel("semCreate");
    result = semCreateReal((long) args->arg1);
    
    args->arg4 = (void *) (long) result;
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
 *  Arguments:  int count   -- the initial count of the semaphore
 *
 *  Return: the id of the semaphore if created, -1 if error
 */

int semCreateReal(int count){
    int i, semID;
    
    checkKernel("semCreateReal");
    
    MboxSend(mutexSem, NULL, 0);
    
    // cheksums: semaphore must be non-negative and sem table must have space for it
    if(semCount >= MAXSEMS){
        if(DEBUG3 && debugSemCreate){
            USLOSS_Console("semCreateReal: sem table full\n");
        }
        MboxCondReceive(mutexSem, NULL, 0);
        return -1;
    }
    
    if(count < 0){
        if(DEBUG3 && debugSemCreate){
            USLOSS_Console("semCreateReal: %d is negative, must be non-negative\n", count);
        }
        MboxCondReceive(mutexSem, NULL, 0);
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
    
    if(DEBUG3 && debugSemCreate){
        USLOSS_Console("Semaphore %d created\n", semID);
    }
    
    MboxCondReceive(mutexSem, NULL, 0);
    return semID;
}

/*
 *  Routine:  SemP
 *
 *  Description: extract arguments for invoking "P" on a semaphore
 *
 *  Arguments:  systemArgs* args    -- the systemArgs struct containing arguments 
 *                                      for executing a sempahore P function
 *
 *  Return: none
 *  side effect: arg4 stores the return status of freeing a semaphore
 *      0: semaphore P
 *     -1: invalid arguments for semaphore
 */

void semP(systemArgs *args)
{
    int semID = (long) args->arg1;
 
    checkKernel("semP");
    
    if(semID < 0 || semID > MAXSEMS){
        if(DEBUG3 && debugSemP){
            USLOSS_Console("semaphore %d out of range\n", semID);
            args->arg4 = (void *) (long) -1;
        }
    }
    else if(SemTable[semID % MAXSEMS].count == UNUSED){
        if(DEBUG3 && debugSemP){
            USLOSS_Console("semaphore %d not in use\n", semID);
            args->arg4 = (void *) (long) -1;
        }
    }
    else{
        semPReal(semID);
        args->arg4 = 0;
    }
} /* end of SemP */

/*
 *  Routine:  SemPReal
 *
 *  Description: "P" a semaphore. "P" stands for proberen or try, it tries to bypass
 *                  the locking mechanism of a semaphore. if it fails,
 *                  then the process will block until it can
 *
 *  Arguments: int semID    -- id of the semaphore handle
 *
 *  Return: none
 *  side effect: will block if fails
 *              may terminate if zapped or freed
 */

void semPReal(int semID){
    
    procPtr Current = &ProcTable[getpid() % MAXPROC];

    checkKernel("semPReal");
    
    MboxSend(mutexSem, NULL, 0);
    
    semPtr sem = &SemTable[semID % MAXSEMS];
    
    if(sem->count == 0){
        if(DEBUG3 && debugSemP){
            USLOSS_Console("semaphor %d at 0, proc %d will block\n", semID, getpid());
        }

        addToSemList(sem, Current);
        
        // if process was zapped or the semaphore was free, terminate
        if(isZapped()){
            if(DEBUG3 && debugSemP){
                USLOSS_Console("semPReal: process %d was zapped\n", Current->pid);
            }
            terminateReal(1);
        }
        
        if(sem->semID == UNUSED){
            if(DEBUG3 && debugSemP){
                USLOSS_Console("semPReal: semaphore %d was freed %d\n", semID);
            }
            terminateReal(1);
        }
        
    }
    if(sem->count > 0)
        sem->count--;
    MboxCondReceive(mutexSem, NULL, 0);
}

/*
 *  Routine:  SemV
 *
 *  Description: extract arguments for invoking "V" on a semaphore
 *
 *  Arguments:  systemArgs* args    -- the systemArgs struct conataining arguments
 *                                      for executing a semaphore "V" function
 *
 *  Return: none
 *  side effect: arg4 stores the return status of freeing a semaphore
 *      0: semaphore V'd
 *     -1: invalid arguments for semaphore
 */

void semV(systemArgs *args)
{
    int semID = (long) args->arg1;
    
    checkKernel("semV");
    if(semID < 0 || semID > MAXSEMS){
        if(DEBUG3 && debugSemP){
            USLOSS_Console("semaphore %d out of range\n", semID);
            args->arg4 = (void *) (long) -1;
        }
    }
    else if(SemTable[semID % MAXSEMS].count == UNUSED){
        args->arg4 = (void *) (long) -1;
    }
    else{
        semVReal(semID);
        args->arg4 = 0;
    }
} /* end of SemV */

/*
 *  Routine:  SemVReal
 *
 *  Description: "V" a semaphore. "V" stands for verhogen or increase, it 
 *                increases the number of processes allowed past the locking mechanism
 *                and unblocks the next process that is P blocked
 *
 *  Arguments:  int semID   -- id of the semaphore handle
 *
 *  Return: none
 *  side effect: next "P" blocked process on semaphore will unblock
 */

void semVReal(int semID){
    

    checkKernel("semVReal");
    
    MboxSend(mutexSem, NULL, 0);
    semPtr sem = &SemTable[semID % MAXSEMS];
    //increase sem counter
    sem->count++;

    //unblock next sem blocked proc
    removeFromSemList(sem);
    MboxCondReceive(mutexSem, NULL, 0);
}

/*
 *  Routine:  SemFree
 *
 *  Description: extract arguments for freeing a semaphore
 *
 *  Arguments: systemArgs* args -- the systemArgs struct containing arguments for
 *                                  freeing a semaphore.
 *
 *  Return: none
 *  side effect: arg4 stores the return status of freeing a semaphore
 *      0: semaphore freed no blocked processes
 *      1: semaphore freed, blocked processes
 *     -1: invalid arguments
 */

void semFree(systemArgs *args)
{
    int semID = (long) args->arg1;
    
    checkKernel("semFree");
    if(semID < 0 || semID > MAXSEMS){
        if(DEBUG3 && debugSemP){
            USLOSS_Console("semaphore %d out of range\n", semID);
            args->arg4 = (void *) (long) -1;
        }
    }
    else if(SemTable[semID % MAXSEMS].count == UNUSED){
        args->arg4 = (void *) (long) -1;
    }
    else{
        args->arg4 = (void *) (long) semFreeReal(semID);
    }
} /* end of SemFree */

/*
 *  Routine:  SemFreeReal
 *
 *  Description: Free a semaphore. Terminate any blocked processes on the semaphore
 *
 *  Arguments:  int semID   -- id of the semaphore handle
 *
 *  Return: 0 if semaphore had not blocked processes, 1 otherwise
 */

int semFreeReal(int semID){
    int result = 0;
//    printSemList(sem);
    checkKernel("semFreeReal");
    MboxSend(mutexSem, NULL, 0);
    
    semPtr sem = &SemTable[semID % MAXSEMS];

    // if processes are blocked on the semaphore return 1
    if(sem->blockedPtr != NULL){
        result = 1;
    }
    
    // clear the semaphore entry
    sem->semID = UNUSED;
    
    // decrement the sem counter
    semCount--;
    
    // unblock any blocked processes
    while(sem->blockedPtr != NULL)
        removeFromSemList(sem);
    
    clearSem(semID);
    
    MboxCondReceive(mutexSem, NULL, 0);
    return result;
}

/*
 *  Routine:  GetTimeofDay
 *
 *  Description: extract arguments for getting the time of day
 *
 *  Arguments: systemArgs args  -- the systemArg struct containing arguments 
 *                                 for getting the time of day
 *
 *  Return: None
 *  side effect: arg1 stores the time of day
 */

void getTimeofDay(systemArgs *args)
{
    checkKernel("getTimeofDay");
    args->arg1 = (void *) (long) getTimeofDayReal();
} /* end of GetTimeofDay */


/*
 *  Routine:  GetTimeofDayReal
 *
 *  Description: Get the time of day
 *
 *  Arguments:  None
 *
 *  Return the time of day
 */
int getTimeofDayReal(){
    checkKernel("getTimeofDayReal");
    return USLOSS_Clock();
}

/*
 *  Routine:  CPUTime
 *
 *  Description: extract arguments for getting the process' CPU time
 *
 *  Arguments: systemArgs* args -- the systemArgs struct for getting the cpu time
 *
 *  Return: none
 *  side effect: arg1 stores the total cpu time
 */

void cpuTime(systemArgs *args)
{
    checkKernel("cputTime");
    args->arg1 = (void *) (long) cpuTimeReal();
} /* end of CPUTime */

/*
 *  Routine:  CPUTimeReal
 *
 *  Description: Get the total cpu time
 *
 *  Arguments: None
 *
 *  Return: the total cpu time
 */

int cpuTimeReal(){
    checkKernel("cputTimeReal");
    return readtime();
}

/*
 *  Routine:  GetPID
 *
 *  Description: extract arguments for getting the process' PID
 *
 *  Arguments: systemArgs* args -- the systemArgs struct for getting the pid
 *
 *  Return: None
 *  Side effect: arg1 will have the pid of the current running process
 */

void getPID(systemArgs *args)
{
    checkKernel("getPID");
    args->arg1 = (void *) (long) getPIDReal();
} /* end of GetPID */

/*
 *  Routine:  GetPIDReal
 *
 *  Description: Get the pid of the current running process
 *
 *  Arguments: None
 *
 *  Return: the pid of the current running process
 */

int getPIDReal(){
    checkKernel("getPIDReal");
    return getpid();
}

/*
 *  Routine:  checkKernel
 *
 *  Description: check kernel mode, halt if not in kernel mode
 *
 *  Arguments: char *name   -- the invoking method
 *
 *  Return: None
 *  side effect: simulator will halt if not in kernel mode
*/
 
void checkKernel(char *name){
    int byte = USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet();
    if(DEBUG3 && debugKernel){
        USLOSS_Console("checkKernelMode: from %s byte: %d\n", name, byte);
    }
    if(byte != 1){
        if(DEBUG3 && debugKernel){
            USLOSS_Console("%s: not in kernel mode\n", name);
        }
        USLOSS_Halt(1);
    }
}

/*
 *  Routine:  changeMode
 *
 *  Description: check from kernel mode to user mode
 *
 *  Arguments: none
 *
 *  Return: None
 *  side effect: simulator will halt if not in kernel mode
 */


void changeMode(){
    USLOSS_PsrSet(USLOSS_PsrGet() - USLOSS_PSR_CURRENT_MODE);
}

/*
 *  Routine:  addSpawn
 *
 *  Description: add a spawned process to the process tree of spawns
 *
 *  Arguments: procPtr new  -- the process to add
 *
 *  Return: None
 *  side effect: none
 */

void addSpawn(procPtr new){
    procPtr Current = &ProcTable[getpid() % MAXPROC];
    procPtr cur = Current->childProc;
    if(cur == NULL){
        // if first child
        Current->childProc = new;
    }
    else{
        for(; cur->nextSiblingProc != NULL; cur = cur->nextSiblingProc);
        cur->nextSiblingProc = new;
    }
}

/*
 *  Routine:  removeSpawn
 *
 *  Description: remove spawn process from spawn process tree
 *
 *  Arguments: 
 *              procPtr parent  -- the parent process of the spawn
 *              procPtr spawn   -- the spawn to remove
 *
 *  Return: None
 *  side effect: none
 */

void removeSpawn(procPtr parent, procPtr spawn){
    procPtr cur = parent->childProc;
    procPtr prev;
    // if first child
    if(cur != NULL){
        if(cur->pid == spawn->pid){
            //update parent
            parent->childProc = cur->nextSiblingProc;
            //update sibling
            cur->nextSiblingProc = NULL;
        }
        // if sibling
        else{
            for(; cur->nextSiblingProc != NULL; cur = cur->nextSiblingProc){
                if(cur->nextSiblingProc->pid == spawn->pid){
                    prev = cur;
                    //update sibling
                    cur = cur->nextSiblingProc;
                    prev->nextSiblingProc = cur->nextSiblingProc;
                    cur->nextSiblingProc = NULL;
                    return;
                }
            }
        }
    }
}

/*
 *  Routine: addToSemList
 *
 *  Description: Add a P blocked process to the semaphore block list
 *
 *  Arguments: 
 *              semPtr sem      -- the semaphore
 *              procPtr proc    -- the blocked process
 *
 *  Return: None
 *  side effect: process will block after adding to sem list
 *               since process may block, mutex is released before 
 *               new semaphore process is invoked
 */

void addToSemList(semPtr sem, procPtr proc){
    procPtr cur = NULL;
    // first process
    if(sem->blockedPtr == NULL){
        sem->blockedPtr = proc;
    }
    else{
        for(cur = sem->blockedPtr; cur->nextBlockedPtr != NULL; cur = cur->nextBlockedPtr);
        cur->nextBlockedPtr = proc;
    }
    proc->sem = sem;
    if(DEBUG3 && debugSemP)
        USLOSS_Console("Receiving on %d proc %d\n", proc->mboxID, proc->pid);
    MboxCondReceive(mutexSem, NULL, 0);
    MboxReceive(proc->mboxID, NULL, 0);
}

/*
 *  Routine:  removeFromSemList
 *
 *  Description: remove the current process from a semaphore's blocked list
 *
 *  Arguments: semPtr sem   -- the semaphore
 *
 *  Return: None
 *  side effect: unblock the process after removing
 */

void removeFromSemList(semPtr sem){
    procPtr cur = sem->blockedPtr;
    if(cur != NULL){
        sem->blockedPtr = cur->nextBlockedPtr;
        cur->nextBlockedPtr = NULL;
        cur->sem = NULL;
        if(DEBUG3 && (debugSemV || debugTerminate || debugSemFree))
            USLOSS_Console("Sending to %d proc %d\n", cur->mboxID, cur->pid);
        MboxCondSend(cur->mboxID, NULL, 0);
    }
}

/* ------------------------------------------------------------------------
 Name - nullsys3
 Purpose - prints error message and terminates
 Parameters - one
 Returns - None
 Side Effects - None
 ----------------------------------------------------------------------- */
void nullsys3(systemArgs *args)
{
    USLOSS_Console("nullsys3(): Invalid syscall %d. Halting...\n", args->number);
    terminateReal(1);
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
    ProcTable[idx].childProc = NULL;
    ProcTable[idx].sem = NULL;
    ProcTable[idx].nextSiblingProc = NULL;
    ProcTable[idx].parentPtr = NULL;
    ProcTable[idx].nextBlockedPtr = NULL;
}

/* ------------------------------------------------------------------------
 Name - clearSem
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
 Name - clearSyscall
 Purpose - clear process in the process table
 Parameters - the index of the process in the process table
 Returns - none
 Side Effects - none
 ------------------------------------------------------------------------ */
void clearSyscall(int idx){
    systemCallVec[idx] = nullsys3;
}

/*
 *  Routine:  printFamily (DEBUG ONLY)
 *
 *  Description: print family relations of spawn process tree
 *
 *  Arguments: \procPtr root    -- the root of the tree to print
 *
 *  Return: None
 *  side effect: None
 */

void printFamily(procPtr root){
    procPtr cur;
    if(root == NULL){
        return;
    }
    if(root->childProc != NULL){
        USLOSS_Console("Parent: %d\n", root->pid);
        printFamily(root->childProc);
    }
    
    // go through siblings
    for(cur = root; cur != NULL; cur = cur->nextSiblingProc){
        //if sibling has child zap child's family
        if(cur->childProc != NULL){
            printFamily(cur->childProc);
        }
        USLOSS_Console("|pid %d|", cur->pid);
        if(cur->nextSiblingProc != NULL)
            USLOSS_Console("->");
    }
    USLOSS_Console("\n");
}

/*
 *  Routine:  printSemList (DEBUG ONLY)
 *
 *  Description: print processes blocked on a semaphore
 *
 *  Arguments: semPtr sem      -- the semaphore
 *
 *  Return: None
 *  side effect: none
 */

void printSemList(semPtr sem){
    procPtr cur;
    if(sem != NULL){
        USLOSS_Console("|Sem %d|->", sem->semID);
        for(cur = sem->blockedPtr; cur != NULL; cur = cur->nextBlockedPtr){
            USLOSS_Console("|pid %d|", cur->pid);
            if(cur->nextBlockedPtr != NULL)
                USLOSS_Console("->");
        }
        USLOSS_Console("\n");
    }
}

