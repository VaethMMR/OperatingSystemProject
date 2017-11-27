/* ------------------------------------------------------------------------
   phase1.c

   University of Arizona
   Computer Science 452 
   Fall 2016
   
   Phase 1
   Due Date: 9/16/16, 9:00 PM
   Sean Vaeth, Brandon Wong
   ------------------------------------------------------------------------ */

#include "phase1.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "kernel.h"
#include "usloss.h"

/* ------------------------- Prototypes ----------------------------------- */
int sentinel (char *);
extern int start1 (char *);
int inKernel();
void disableInterrupts(void);
void enableInterrupts(void);

int fork1(char *, int (*)(char *), char *, int, int);
int join(int *);
void quit(int);

void dispatcher(void);
procPtr scheduler(void);

void launch();
int getpid(void);

void dumpProcesses(void);
void printReadyList(void);

static void checkDeadlock();

int blockMe(int);
int unblockProc(int);

int readCurStartTime(void);
int readTime();
void timeSlice(void);

int zap(int);
int isZapped();

void clockHandler(int, void*);

void cleanProcData(int);
void cleanTableEntry(int);

void enqueueToReadyList(procPtr);
void dequeueFromReadyList(procPtr);


/* -------------------------- Globals ------------------------------------- */
/* Patrick's debugging global variable... */
int debugFork = 0;
int debugJoin = 0;
int debugDispatcher = 0;
int debugQuit = 0;
int debugStartup = 0;
int debugLaunch = 0;
int debugInterrupts = 0;
int debugKernel = 0;
int debugFinish = 0;
int debugZap = 0;
int debugScheduler = 0;
int debugSentinel = 0;
int debugCleanTableEntry = 0;
int debugQueue = 0;
int debugBlockMe = 0;
int debugUnblock = 0;

/* the process table */
procStruct ProcTable[MAXPROC];
procStruct nullProc;

/* Process lists  */
static procPtr ReadyList[SENTINELPRIORITY];

/* current process ID */
procPtr Current = NO_CURRENT_PROCESS;

/*Empty Process*/
procStruct null;

/* the next pid to be assigned */
unsigned int nextPid = SENTINELPID;

/* Count of the process table */
int tableSize = 0;

/* pid count*/
int pid = SENTINELPID;

int SENTINELFLAG = 0;
/* -------------------------- Functions ----------------------------------- */
/* ------------------------------------------------------------------------
   Name - startup
   Purpose - Initializes process lists and clock interrupt vector.
             Start up sentinel process and the test process.
   Parameters - none, called by USLOSS
   Returns - nothing
   Side Effects - lots, starts the whole thing
   ----------------------------------------------------------------------- */
void startup(){
    int result; // value returned by call to fork1()
    int i;
    
    // initialize the process table
    if (DEBUG && debugStartup)
        USLOSS_Console("startup(): initializing process table, ProcTable[]\n");
    
    for(i = 0; i < 50; i++){
        cleanProcData(i);
        cleanTableEntry(i);
    }
    
    if(DEBUG && debugStartup){
        USLOSS_Console("startup: print table\n");
        dumpProcesses();
    }

    // Initialize the Ready list, etc.
    if (DEBUG && debugStartup)
        USLOSS_Console("startup(): initializing the Ready list\n");
    
    for(i = 0; i < SENTINELPRIORITY; i++){
        ReadyList[i] = NULL;
    }

    // Initialize the clock interupt vector
    USLOSS_IntVec[USLOSS_CLOCK_INT] = clockHandler;

    // startup a sentinel process
    if (DEBUG && debugStartup)
        USLOSS_Console("startup(): calling fork1() for sentinel\n");
    result = fork1("sentinel", sentinel, NULL, USLOSS_MIN_STACK,
                    SENTINELPRIORITY);
    if (result < 0) {
        if (DEBUG && debugStartup) {
            USLOSS_Console("startup(): fork1 of sentinel returned error, ");
            USLOSS_Console("halting...\n");
        }
        USLOSS_Halt(1);
    }
  
    // start the test process
    if (DEBUG && debugStartup)
        USLOSS_Console("startup(): calling fork1() for start1\n");
    result = fork1("start1", start1, NULL, 2 * USLOSS_MIN_STACK, 1);
    if (result < 0) {
        USLOSS_Console("startup(): fork1 for start1 returned an error, ");
        USLOSS_Console("halting...\n");
        USLOSS_Halt(1);
    }

    USLOSS_Console("startup(): Should not see this message! ");
    USLOSS_Console("Returned from fork1 call that created start1\n");

    return;
} /* startup */

/* ------------------------------------------------------------------------
   Name - finish
   Purpose - Required by USLOSS
   Parameters - none
   Returns - nothing
   Side Effects - none
   ----------------------------------------------------------------------- */
void finish(void)
{
    if (DEBUG && debugFinish){
        USLOSS_Console("in finish...\n");
        USLOSS_Console("finish(): One last dumpProcesses\n");
        dumpProcesses();
        USLOSS_Console("finish(): One last print readylist\n");
        printReadyList();
    }
} /* finish */

/* ------------------------------------------------------------------------
   Name - fork1
   Purpose - Gets a new process from the process table and initializes
             information of the process.  Updates information in the
             parent process to reflect this child process creation.
   Parameters - the process procedure address, the size of the stack and
                the priority to be assigned to the child process.
   Returns - the process id of the created child or -1 if no child could
             be created or if priority is not between max and min priority.
   Side Effects - ReadyList is changed, ProcTable is changed, Current
                  process information changed
   ------------------------------------------------------------------------ */
int fork1(char *name, int (*startFunc)(char *), char *arg, int stacksize, int priority){
    int procSlot = -1;

    // test if in kernel mode; halt if in user mode
    if(!inKernel()){
        if(DEBUG && debugFork){
            USLOSS_Console("fork1: not in kernel mode\n");
        }
        USLOSS_Console("fork1(): called while in user mode, by process %d. Halting...\n", Current->pid);
        USLOSS_Halt(1);
    }
    
    disableInterrupts();
    
    if (DEBUG && debugFork)
        USLOSS_Console("fork1(): creating process %s\n", name);

    // Checks if the table has room
    if(tableSize > MAXPROC){
        if(DEBUG && debugFork){
            USLOSS_Console("fork1(): Process Table is full.  Halting...\n");
        }
        USLOSS_Halt(1);
    }
    
    //checks if func is NULL
    if(startFunc == NULL){
        if(DEBUG && debugFork){
            USLOSS_Console("fork1(): Function is null.  Halting...\n");
        }
        USLOSS_Halt(1);
    }
    
    // Checks if the name is NULL
    if(name == NULL){
        if(DEBUG && debugFork){
            USLOSS_Console("fork1(): Process name is null.  Halting...\n");
        }
        USLOSS_Halt(1);
    }
    
    // Checks if sentinel has the correct priority
    if(strcmp(name, "sentinel") == 0 && priority != SENTINELPRIORITY){
        if(DEBUG && debugFork){
            USLOSS_Console("Sentinel priority invalid");
        }
        USLOSS_Halt(1);
    }
    
    // Checks if the priority is in the bounds 1(MAX) <= priority <= 5(MIN)
    if(priority < MAXPRIORITY || (priority > MINPRIORITY && strcmp(name, "sentinel") != 0)){
        if(DEBUG && debugFork){
            USLOSS_Console("fork1(): Process priority is out of bounds. Returning -1\n");
        }
        return  -1;
    }
    
    if(strcmp(name, "sentinel") == 0 && SENTINELFLAG == 1){
        if(DEBUG && debugFork){
            USLOSS_Console("Attempted to fork 2 sentinels. Halting...");
        }
        USLOSS_Halt(1);
    }

   if(pid == SENTINELPID)
	SENTINELFLAG = 1;
    
    // Checks to see if it is another start1 by checking the space in the table where it belongs at spot 1
    if(strcmp(ProcTable[1].name, "start1") == 0){
        if(DEBUG && debugFork){
            USLOSS_Console("fork1(): Second start1 created.  Halting...\n");
        }
        USLOSS_Halt(1);
    }
    
    // Checks to see if the size acceptable to the USLOSS Stack
    if(stacksize < USLOSS_MIN_STACK){
        if(DEBUG && debugFork)
            USLOSS_Console("fork1(): Process stack below the Size.  Halting...\n");
        return -2;
    }
    // fill-in entry in process table */
    if ( strlen(name) >= (MAXNAME - 1) ) {
        if(DEBUG && debugFork){
            USLOSS_Console("fork1(): Process name is too long.  Halting...\n");
        }
        USLOSS_Halt(1);
    }
    
    // Insert proc to process table
    int i = 0;
    for(; ProcTable[pid % MAXPROC].status != UNUSED; pid++){
        i++;
        if(i == 50){
            if(DEBUG && debugFork)
                USLOSS_Console("fork1(): Something went horribly wrong\n");
            return -1;
        }
    }

    procSlot = pid % MAXPROC;
    
    //initialize process
    strcpy(ProcTable[procSlot].name, name);
    ProcTable[procSlot].stack = malloc(stacksize);
    ProcTable[procSlot].tableSlot = procSlot;
    ProcTable[procSlot].status = READY;
    ProcTable[procSlot].timeslice = TIMESLICE;
    ProcTable[procSlot].priority = priority;
    ProcTable[procSlot].pid = pid;
    ProcTable[procSlot].stackSize = stacksize;
    ProcTable[procSlot].startFunc = startFunc;
    
    ProcTable[procSlot].nextProcPtr = NULL;
    ProcTable[procSlot].nextSiblingPtr = NULL;
    ProcTable[procSlot].childProcPtr = NULL;
    ProcTable[procSlot].nextDeadChild = NULL;
    ProcTable[procSlot].nextDeadSibling = NULL;

    ProcTable[procSlot].startTime = -1;
    ProcTable[procSlot].sliceCount = 0;
    
    ProcTable[procSlot].numChildren = 0;
    ProcTable[procSlot].numJoinedChildren = 0;
    ProcTable[procSlot].tableChildren = 0;

    ProcTable[procSlot].isZapped = FALSE;
    
    if ( arg == NULL )
        ProcTable[procSlot].startArg[0] = '\0';
    else if ( strlen(arg) >= (MAXARG - 1) ) {
        if(DEBUG && debugFork){
            USLOSS_Console("fork1(): argument too long.  Halting...\n");
        }
        USLOSS_Halt(1);
    }
    else
        strcpy(ProcTable[procSlot].startArg, arg);

    // Initialize context for this process, but use launch function pointer for
    // the initial value of the process's program counter (PC)
    USLOSS_ContextInit(&(ProcTable[procSlot].state), USLOSS_PsrGet(),
                       ProcTable[procSlot].stack,
                       ProcTable[procSlot].stackSize,
                       launch);

    // for future phase(s)
    p1_fork(ProcTable[procSlot].pid);

    // More stuff to do here...
    procPtr ptr = &ProcTable[procSlot] , cur;
    
    
    //////////////////////////  Build Proc Tree  /////////////////////////
    if(DEBUG && debugFork){
        USLOSS_Console("%s forked %d\n", ptr->name, ptr->pid);
        dumpProcesses();
    }
    
    if(Current != NO_CURRENT_PROCESS){
        ptr->parentPtr = Current;
        ptr->nextSiblingPtr = NULL;
        ptr->ppid = Current->pid;
        if(Current->childProcPtr == NULL){
            Current->childProcPtr = ptr;
        }
        else{
            for(cur = Current->childProcPtr; cur->nextSiblingPtr != NULL; cur = cur->nextSiblingPtr);
            cur->nextSiblingPtr = ptr;
        }
    }
    
    //////////////////////////  Start Ready Lists  /////////////////////////
    /* put proc in the ready list */
    enqueueToReadyList(ptr);
    
    /////////////////////////  Finish Ready List  ///////////////////////
    // Update counters
    tableSize++;	// tableSize
	
    int curPid = pid;	// pid
    pid++;
    
    if(Current != NO_CURRENT_PROCESS){
        //active children
        Current->numActiveChildren++;
    
        //children
        Current->numChildren++;

	//children visible in the table
	Current->tableChildren++;
    }
    
    if(curPid != SENTINELPID){
        if(debugFork && DEBUG)
            USLOSS_Console("fork1(): calling dispatcher Result %d\n", curPid);
        dispatcher();
    }
    
    // Enable interupts
    enableInterrupts();
    
    return curPid;
} /* fork1 */

/* ------------------------------------------------------------------------
   Name - launch
   Purpose - Dummy function to enable interrupts and launch a given process
             upon startup.
   Parameters - none
   Returns - nothing
   Side Effects - enable interrupts
   ------------------------------------------------------------------------ */
void launch()
{
    int result;

    if (DEBUG && debugLaunch)
        USLOSS_Console("launch(): started\n");
    
    //Activate timeslice
    Current->startTime = USLOSS_Clock();

    // Enable interrupts
    enableInterrupts();

    // Call the function passed to fork1, and capture its return value
    result = Current->startFunc(Current->startArg);

    if (DEBUG && debugLaunch)
        USLOSS_Console("Process %d returned to launch\n", Current->pid);

    quit(result);

} /* launch */


/* ------------------------------------------------------------------------
   Name - join
   Purpose - Wait for a child process (if one has been forked) to quit.  If 
             one has already quit, don't wait.
   Parameters - a pointer to an int where the termination code of the 
                quitting process is to be stored.
   Returns - the process id of the quitting child joined on.
             -1 if the process was zapped in the join
             -2 if the process has no children
   Side Effects - If no child process has quit before join is called, the 
                  parent is removed from the ready list and blocked.
   ------------------------------------------------------------------------ */
int join(int *code)
{
    if(DEBUG && debugJoin){
        USLOSS_Console("join(): start of join\n");
        USLOSS_Console("join(): Current.name: %s\n", Current -> name);
        USLOSS_Console("join(): Current.numChildren: %d\n", Current->numChildren);
        USLOSS_Console("join(): Current.numDeadChildren: %d\n", Current->numDeadChildren);
        USLOSS_Console("join(): Current.numZombieChildren: %d\n", Current->numZombieChildren);
        dumpProcesses();
    }
    
    // test if in kernel mode; halt if in user mode
    if(!inKernel()){
        if(DEBUG && debugJoin){
            USLOSS_Console("join: not in kernel mode\n");
        }
        USLOSS_Halt(1);
    }
    
    int pid = -1;
    /* CASE 0 : no children */
    if(Current->numChildren == 0){
        if(DEBUG && debugJoin){
            USLOSS_Console("join(): case 0: no children\n");
        }
        return -2;
    }
    
    /* CASE 1: all children joined */
    if(Current->numChildren == Current->numJoinedChildren){
        if(DEBUG && debugJoin){
            USLOSS_Console("Join(): case1: all children joined\n");
        }
        return -2;
    }
    
    // CASE 2 : Child(ren) quit before call to join
    if(Current->numZombieChildren > 0){
        if(DEBUG && debugJoin){
            USLOSS_Console("join(): case 2: %s quitting before join\n", Current->name);
            USLOSS_Console("join(): pid = %d\n", Current->nextZombieChild->pid);
        }
        Current->numZombieChildren--;
        *code = Current->nextZombieChild->deadStatus;
        pid = Current->nextZombieChild->pid;

        procPtr rem, cur = NULL;
        //Remove from tree
        if(Current->childProcPtr->nextSiblingPtr == NULL){
            Current->childProcPtr = NULL;
        }
        else if(Current->childProcPtr->pid == pid){
            cur = Current->childProcPtr;
            Current->childProcPtr = cur->nextSiblingPtr;
            cur->nextSiblingPtr = NULL;
            
            if(DEBUG && debugJoin){
                for(cur = Current->childProcPtr; cur->nextSiblingPtr != NULL && cur->nextSiblingPtr->pid != pid; cur = cur->nextSiblingPtr){
                    USLOSS_Console("CUR: %s %d PID %d\n", cur->name, cur->pid, cur->pid);
                }
            }
        }
        else{
                for(cur = Current->childProcPtr; cur->nextSiblingPtr != NULL && cur->nextSiblingPtr->pid != pid; cur = cur->nextSiblingPtr){
                    if(DEBUG && debugJoin){
                        USLOSS_Console("CUR: %s %d PID %d\n", cur->name, cur->pid, cur->pid);
                    }
            }
            if(cur->nextSiblingPtr != NULL && cur->nextSiblingPtr->pid == pid){
                rem = cur->nextSiblingPtr;
                cur->nextSiblingPtr = rem->nextSiblingPtr;
                rem->nextSiblingPtr = NULL;
            }
            if(DEBUG && debugJoin){
                for(cur = Current->childProcPtr; cur->nextSiblingPtr != NULL && cur->nextSiblingPtr->pid != pid; cur = cur->nextSiblingPtr){
                    USLOSS_Console("CUR: %s %d PID %d\n", cur->name, cur->pid, cur->pid);
                }
            }
        }
        
        //Finish cleaning zombie
        
        
        //Dequeue zombie child
        rem = Current->nextZombieChild;
        Current->nextZombieChild = rem->nextZombieSibling;
        rem->nextZombieSibling = NULL;
        
        //increase joined counter
        Current->numJoinedChildren++;
        
        //// Clean everything remaining
        cleanTableEntry(rem->tableSlot);

        
        //if process was zapped while waiting
        if(Current->isZapped == TRUE){
            if(debugJoin && DEBUG)
                USLOSS_Console("Process %s was zapped. Returning -1\n", Current->name);
            return -1;
        }
        
        return pid;
    }
    
    // CASE 3 : No child(ren) have quit
    if(Current->numActiveChildren > 0){
        Current->status = JOINBLOCKED;
        
        if(DEBUG && debugJoin){
            dumpProcesses();
        }
        dispatcher();
        
        if (debugJoin && DEBUG) {
            USLOSS_Console("join(): after dispatcher\n");
            USLOSS_Console("join(): Current(%s) dead child (%s) pid: %d\n", Current->name, Current->nextDeadChild->name, Current->nextDeadChild->pid);
            USLOSS_Console("join(): table where dead child is: %d \n", Current->nextDeadChild->tableSlot);
            USLOSS_Console("join(): Current.name: %s\n", Current -> name);
            USLOSS_Console("join(): Current.numChildren: %d\n", Current->numChildren);
            USLOSS_Console("join(): Current.numDeadChildren: %d\n", Current->numDeadChildren);
            USLOSS_Console("join(): Current.numZombieChildren: %d\n", Current->numZombieChildren);
        }
        
        // Need to take the dead child off the table and get it ready;
        // Opens the space for the table
        pid = Current->nextDeadChild->pid;
        *code = Current->nextDeadChild->deadStatus;
        
        procPtr rem, cur = NULL;
        // Remove from tree
        if(Current->childProcPtr->nextSiblingPtr == NULL){
            Current->childProcPtr = NULL;
        }
        else if(Current->childProcPtr->pid == pid){
            cur = Current->childProcPtr;
            Current->childProcPtr = cur->nextSiblingPtr;
            cur->nextSiblingPtr = NULL;
            
            if(DEBUG && debugJoin){
                for(cur = Current->childProcPtr; cur->nextSiblingPtr != NULL && cur->nextSiblingPtr->pid != pid; cur = cur->nextSiblingPtr){
                    USLOSS_Console("CUR: %s %d PID %d\n", cur->name, cur->pid, cur->pid);
                }
            }
        }
        else{
            for(cur = Current->childProcPtr; cur->nextSiblingPtr != NULL && cur->nextSiblingPtr->pid != pid; cur = cur->nextSiblingPtr){
                if(DEBUG && debugJoin){
                    USLOSS_Console("CUR: %s %d PID %d\n", cur->name, cur->pid, cur->pid);
                }
            }
            if(cur->nextSiblingPtr != NULL && cur->nextSiblingPtr->pid == pid){
                rem = cur->nextSiblingPtr;
                cur->nextSiblingPtr = rem->nextSiblingPtr;
                rem->nextSiblingPtr = NULL;
            }
            if(DEBUG && debugJoin){
                for(cur = Current->childProcPtr; cur->nextSiblingPtr != NULL && cur->nextSiblingPtr->pid != pid; cur = cur->nextSiblingPtr){
                    USLOSS_Console("CUR: %s %d PID %d\n", cur->name, cur->pid, cur->pid);
                }
            }
        }
        
//// Clean everything remaining
        cleanTableEntry(Current->nextDeadChild->tableSlot);
		
        if (debugJoin && DEBUG)
            USLOSS_Console("join(): case 3: after %s cleaned tableSlot %d\n", Current->nextDeadChild->name, Current->nextDeadChild->tableSlot);
    }
    
    //increase joined counter
    Current->numJoinedChildren++;
    
    //if process was zapped while waiting
    if(Current->isZapped == TRUE){
        if(debugJoin && DEBUG)
            USLOSS_Console("Process %s was zapped. Returning -1\n", Current->name);
        return -1;
    }
    
    return pid;
} /* join */


/* ------------------------------------------------------------------------
   Name - quit
   Purpose - Stops the child process and notifies the parent of the death by
             putting child quit info on the parents child completion code
             list.
   Parameters - the code to return to the grieving parent
   Returns - nothing
   Side Effects - changes the parent of pid child completion status list.
   ------------------------------------------------------------------------ */
void quit(int code)
{
    // when a process quits does it remove the join flags on the siblings?
    procPtr cur, prev, debug;
    
    if(debugQuit && DEBUG)
        USLOSS_Console("quit(): %s is here\n", Current->name);
    
    // test if in kernel mode; halt if in user mode
    if(!inKernel()){
        if(DEBUG && debugQuit){
            USLOSS_Console("quit: not in kernel mode\n");
        }
        USLOSS_Console("quit(): called while in user mode, by process %d. Halting...\n", Current->pid);
        USLOSS_Halt(1);
    }
    
    disableInterrupts();
    
    // Checks for children
    if(Current->numActiveChildren > 0){
        USLOSS_Console("quit(): process %d, '%s', has active children. Halting...\n", Current->pid, Current->name);
        USLOSS_Halt(1);
    }
    
    ////////////////////////// CASE 0 /////////////////////////
    // CASE 0 : if Current is a parent of dead children, remove them from the proc table entirely
    for(cur = Current->nextZombieChild; cur != NULL; ){
        prev = cur;
        cur = cur->nextZombieSibling;
        prev->nextZombieSibling = NULL;
        cleanProcData(prev->tableSlot);
        cleanTableEntry(prev->tableSlot);
    }
    
    ////////////////////////// CASE 1 /////////////////////////
    // CASE 1 : Checks if the parent has called join
    if(Current -> parentPtr != NULL && Current -> parentPtr->status == JOINBLOCKED){
        if(debugQuit && DEBUG){
            USLOSS_Console("quit(): Case 1: Parent (%s) called join on this (%s)\n", Current ->parentPtr->name, Current->name);
        }
        
        ////////////// PARENT STUFF  //////////////
        // Check for blocked parent and wake it up change status to ready
        // Update it to not being joined
        
        //enqueue to ReadyList
        enqueueToReadyList(Current->parentPtr);
        
        ///////////// END PARENT STUFF //////////////////
        if(debugQuit && DEBUG)
            USLOSS_Console("quit(): Before %s calls p1_quit()\n", Current -> name);
        
        p1_quit(Current -> pid);
        
        if(debugQuit && DEBUG)
            USLOSS_Console("quit(): After %s calls p1_quit()\n", Current -> name);
        
    }
    /////////////// END CASE 1//////////////////////////////
    
    ////////////// CASE 2 : parent hasn't called join() ////////////////////////////
    else{
        if(DEBUG && debugQuit)
            USLOSS_Console("quit(): case 2: parent hasn't called join on %s\n", Current -> name);
        
        //  Sees if there are parents then makes a zombie
        if(Current->parentPtr != NULL){
            Current->status = QUIT;
            Current->parentPtr -> numZombieChildren ++;
            
            //enqueues a zombie
            if(Current->parentPtr->nextZombieChild == NULL){
                Current->parentPtr->nextZombieChild = Current;
                Current->nextZombieSibling = NULL;
            }
            else{
                procPtr cur;
                for(cur = Current->parentPtr->nextZombieChild; cur->nextZombieSibling != NULL; cur = cur->nextZombieSibling);
                cur->nextZombieSibling = Current;
                Current->nextZombieSibling = NULL;
            }
        }
        
        if(debugQuit && DEBUG)
            dumpProcesses();
        
        p1_quit(Current -> pid);
    }
    //////////////////// END CASE 2///////////////////////////////
    
    if(debugQuit && DEBUG)
        USLOSS_Console("quit(): %s After cases\n", Current->name);
    tableSize--;
    
    //add child to parent's dead children with a pointer
    Current->nextDeadSibling = NULL;
    if(Current->parentPtr != NULL){
        if(Current->parentPtr->nextDeadChild == NULL){
            Current->parentPtr->nextDeadChild = Current;
        }
        else{
            Current->nextDeadSibling = Current->parentPtr->nextDeadChild;
            Current->parentPtr->nextDeadChild = Current;
        }
        // Decrements the parents active child count
        Current->parentPtr->numActiveChildren--;
        Current->parentPtr->numDeadChildren++;
        
        if(debugQuit && DEBUG)
            USLOSS_Console("quit(): Parent (%s) dead child (%s)\n", Current-> parentPtr->name, Current->name);
    }

    if(Current->zapPtr != NULL){
        // unblock all zapped procs
        for(cur = Current->zapPtr; cur != NULL; cur = Current->zapPtr){
            if(DEBUG && debugQuit){
                USLOSS_Console("zapped %s zapper(s)\n", Current->name, Current->zapPtr->name);
                for(debug = Current->zapPtr; debug != NULL; debug = debug->nextZapPtr){
                    USLOSS_Console("|%s|", debug->name);
                    if(debug->nextZapPtr != NULL)
                        USLOSS_Console("->");
                }
                USLOSS_Console("\n");
            }
            enqueueToReadyList(cur);
            Current->zapPtr = cur->nextZapPtr;
            cur->nextZapPtr = NULL;
        }
    }
    
    if(debugQuit && DEBUG)
        USLOSS_Console("quit(): %s has quit.  Calling dispatcher()\n", Current->name);
    
	// Set dead values of the process
    Current->deadStatus = code;

// Everything if parent has done a join, almost everything (keep pid and status) if not		
	// clean child from proc table
    Current->status = QUIT;
    cleanProcData(Current->tableSlot);
    
    //If proc has no parent, there will be no join
    //Clear all data from table
    if(Current->parentPtr == NULL){
        cleanTableEntry(Current->tableSlot);
    }
	
    dispatcher();
    
    enableInterrupts();
    
} /* quit */

/* ------------------------------------------------------------------------
   Name - zap
   Purpose - The operation marks a process as being zapped.  Calls to isZapped() by 
			 that process will return 1.  zap(int) does not return until the zapped 
			 process has been called quit(int).
   Parameters - The pid of a process to be marked as zapped.
   Returns - Returns -1 if the calling process itself was zapped while in zap(int).
			 Returns 0 if the zapped process has called quit(int).
   Side Effects - Zombie status processes cannot be zapped and will return -1 if they
				  are indicated to have been zapped.
   ------------------------------------------------------------------------ */
int zap(int pid){
    
    // test if in kernel mode; halt if in user mode
    if(!inKernel()){
        if(DEBUG && debugZap){
            USLOSS_Console("zap: not in kernel mode\n");
        }
        USLOSS_Halt(1);
    }
    
    int i = pid % MAXPROC;
    procPtr brannigan = &ProcTable[i];
    
    if(debugZap && DEBUG){
        USLOSS_Console("In zap(): pid %d\n", i);
    }
    
    if(Current->pid == pid){
        USLOSS_Console("zap(): process %d tried to zap itself.  Halting...\n", pid);
        USLOSS_Halt(1);
    }
    
    if(brannigan->status == QUIT){
        if(DEBUG && debugZap){
            USLOSS_Console("zap(): %s %d already quit\n", brannigan->name, brannigan->pid);
        }
        if(Current->isZapped){
            if(DEBUG && debugZap){
                USLOSS_Console("zap(): %s was zapped while in zap\n", Current->name);
            }
            return -1;
        }
        return 0;
    }
    
    if(brannigan->pid == pid){
        if(DEBUG && debugZap)
            USLOSS_Console("Zap(): %s was zapped %d\n", ProcTable[i].name, i);

        brannigan->isZapped = TRUE;
        Current->status = ZAPBLOCKED;
        
        procPtr cur;
        Current->nextZapPtr = NULL;
        if(brannigan->zapPtr == NULL){
            brannigan->zapPtr = Current;
        }
        else{
            for(cur = brannigan->zapPtr; cur->nextZapPtr != NULL; cur = cur->nextZapPtr);
            cur->nextZapPtr = Current;
        }
        
        dispatcher();
        
        //if process was zapped while in zapped
        if(Current->isZapped == TRUE){
            if(DEBUG && debugZap)
                USLOSS_Console("zap(): %s was zapped while in zap.\n", Current->name);
            return -1;
        }
        
    if(DEBUG && debugZap)
        USLOSS_Console("zap(): %s was zapped\n", brannigan->name);
    return 0;
    }

    USLOSS_Console("zap(): process being zapped does not exist.  Halting...\n", pid);
    USLOSS_Halt(1);
    return -1; //Unreachable but so compiler doesn't complain
}/* zap */

/* ------------------------------------------------------------------------
   Name - isZapped
   Purpose - Determines if the current process is zapped.
   Parameters - none
   Returns - Whether the current process is zapped or not. 
   Side Effects - Halts the process if it is not in kernel mode.
   ------------------------------------------------------------------------ */
int isZapped(){
    // test if in kernel mode; halt if in user mode
    if(!inKernel()){
        if(DEBUG && debugZap){
            USLOSS_Console("zap: not in kernel mode\n");
        }
        USLOSS_Halt(1);
    }
    
    return Current->isZapped;
}/* isZapped */

/* ------------------------------------------------------------------------
   Name - getpid
   Purpose - To return the pid of the current process.
   Parameters - none
   Returns - The pid of the current process.
   Side Effects - none
   ------------------------------------------------------------------------ */
int getpid(void){
    if(!inKernel()){
        if(DEBUG && debugInterrupts){
            USLOSS_Console("getpid: not in kernel mode\n");
        }
        USLOSS_Halt(1);
    }

    return Current->pid;
}/* getpid */

/* ------------------------------------------------------------------------
   Name - dumpProcesses
   Purpose - Prints out the processes to the console.
   Parameters - none
   Returns - none
   Side Effects - Calls USLOSS_Halt(1) if we are not in kernel mode.
   ------------------------------------------------------------------------ */
void dumpProcesses(void){
    int i;
    char *runStatus = "unused";
    int time = USLOSS_Clock();   
 
    if(!inKernel()){
        if(DEBUG && debugKernel){
            USLOSS_Console("dump: not in kernel mode\n");
        }
        USLOSS_Halt(1);
    }
    
    USLOSS_Console("PROCESSES:\n\tNAME\t\tPID\tPPID\t#CHILDREN\tPRIORITY    STATUS\t\tSTART TIME(ms)\tTIMESLICE\tCPU TIME(micros)\tSLICES\n");
    for(i = 0; i < MAXPROC; i++){
        if(ProcTable[i].status == UNUSED)
            runStatus = "Unused";
        if(ProcTable[i].status == READY)
            runStatus = "Ready";
        if(ProcTable[i].status == RUNNING)
            runStatus = "Running";
        if(ProcTable[i].status == JOINBLOCKED)
            runStatus = "JoinBlocked";
        if(ProcTable[i].status == ZAPBLOCKED)
            runStatus = "ZapBlocked";
        if(ProcTable[i].status == QUIT)
            runStatus = "Quit";
        
        USLOSS_Console("%d:\t", i);
        USLOSS_Console(strlen(ProcTable[i].name) > 6 ? "%s\t" : "%s\t\t", ProcTable[i].name);
        USLOSS_Console(ProcTable[i].pid < 0 ? "--\t" : ProcTable[i].pid > 9 ? "%d\t" : "% d\t", ProcTable[i].pid);
        USLOSS_Console(ProcTable[i].ppid < 0 ? "--\t" : ProcTable[i].ppid > 9 ? "%d\t" : "% d\t", ProcTable[i].ppid);
	USLOSS_Console(ProcTable[i].status == UNUSED ? "--\t\t" : "%d\t\t", ProcTable[i].tableChildren);
        USLOSS_Console(ProcTable[i].priority < 0 ? "--\t    " : " %d\t    ", ProcTable[i].priority);
        if(ProcTable[i].status >= BLOCKMEBLOCKED)
            USLOSS_Console("Blocked(%d)\t\t", ProcTable[i].status);
        else
            USLOSS_Console("%s\t\t", runStatus);
        USLOSS_Console(ProcTable[i].status == UNUSED ? "--\t\t" : ProcTable[i].startTime == -1 ? "--\t\t" : "%dms\t\t", ProcTable[i].startTime / 1000);
        USLOSS_Console(ProcTable[i].status == UNUSED ?  "--\t\t" : "%dms\t\t", ProcTable[i].timeslice/1000);
        USLOSS_Console(ProcTable[i].status == UNUSED ? "--\t\t\t" : ProcTable[i].startTime == -1 ? "--\t\t\t" : "%d\t\t\t", (time - ProcTable[i].startTime));
        USLOSS_Console(ProcTable[i].status == UNUSED ? "--\n" : ProcTable[i].sliceCount > 9 ? "%d\n" : "% d\n", ProcTable[i].sliceCount);
    }
    USLOSS_Console("\n");
}/* dumpProcesses */

/* ------------------------------------------------------------------------
   Name - printReadyList
   Purpose - Used for debugging to print out the ready list.
   Parameters - none
   Returns - none
   Side Effects - none
   ------------------------------------------------------------------------ */
void printReadyList(){
    int i;
    USLOSS_Console("********* READY LIST **********\n");
    for(i = 0; i < SENTINELPRIORITY; i++){
        procPtr cur = ReadyList[i];
        USLOSS_Console("At Priority %d\n", i+1);
        while(cur != NULL){
            USLOSS_Console("|%s %d|", cur->name, cur->pid);
            if(cur->nextProcPtr != NULL)
                USLOSS_Console("->");
            cur = cur -> nextProcPtr;
        }
        USLOSS_Console("\n");
        if(cur != NULL)
            USLOSS_Console("    %s\n",cur == NULL ? NULL : cur->name);
        
    }
    USLOSS_Console("********* READY LIST **********\n");
}

/* ------------------------------------------------------------------------
   Name - blockMe
   Purpose - The operation will block the calling process.  The process will be halted
			 if the argument's value is less than or equal to 10.
   Parameters - Value used to indicate the status of the process.
   Returns - Returns -1 if the process was zapped in the process or 0 if otherwise.
   Side Effects - The calling process's status becomes the new status and the dispatcher is called.
   ------------------------------------------------------------------------ */
int blockMe(int newStatus){

    if(!inKernel()){
        if(DEBUG && debugInterrupts){
            USLOSS_Console("blockMe: not in kernel mode\n");
        }
        USLOSS_Halt(1);
    }   
 
    if(newStatus < BLOCKMEBLOCKED){
        if(DEBUG && debugBlockMe){
            USLOSS_Console("Block Me: %s status is %d which is less than 10.\n", Current->name, newStatus);
        }
        USLOSS_Halt(1);
    }
    
    Current->status = newStatus;
    dispatcher();
        
    if(Current->isZapped){
        if(DEBUG && debugBlockMe){
            USLOSS_Console("blockMe(): %s has been zapped while blocked\n", Current->name);
        }
        return -1;
    }
    return 0;
}/* blockMe */

/* ------------------------------------------------------------------------
   Name - unblockProc
   Purpose - This operation unblocks the process pid previously blocked by calling blockMe.
			 The status of that process is changed to ready and is put on the Ready List.
   Parameters - The pid of the process previously blocked by blockMe.
   Returns - Returns -2 if the indicated process was not blocked, does not exist, is the current
			 process, or is blocked on a status less than or equal to 10.  Returns -1 if the calling
			 process was zapped.  Returns 0 if otherwise.
   Side Effects - The process is put into the Ready List and dispatcher() is called.
   ------------------------------------------------------------------------ */
int unblockProc(int pid){

    if(!inKernel()){
        if(DEBUG && debugInterrupts){
            USLOSS_Console("unblockProc: not in kernel mode\n");
        }
        USLOSS_Halt(1);
    }
	
    pid = pid % 50;
	
    procPtr ptr = &ProcTable[pid % MAXPROC];
    
    if(ptr->status < BLOCKMEBLOCKED){
        if(DEBUG && debugUnblock){
            USLOSS_Console("unblockProc(): %s has a blocked status less than 10. Will not unblock\n", ptr->name);
        }
        return -2;
    }
    
    if(ptr->pid == Current->pid){
        if(DEBUG && debugUnblock){
            USLOSS_Console("unblockProc(): %s can not unblock itself\n", ptr->name);
        }
        return -2;
    }
    
    if(DEBUG && debugUnblock){
        USLOSS_Console("unblocProc(): unblocking %s %d\n", ptr->name, ptr->pid);
    }
    
    enqueueToReadyList(ptr);
    dispatcher();

	if(Current->isZapped ){
        if(DEBUG && debugUnblock){
            USLOSS_Console("unblockProc(): %s was zapped\n", Current->name);
        }
		return -1;
	}
	
    if(DEBUG && debugUnblock){
        USLOSS_Console("unblockProc(): %s %d has been unblocked by %s %d\n", ptr->name, ptr->pid, Current->name, Current->pid);
        dumpProcesses();
    }
	
    return 0;
}/* unblocProc */

/* ------------------------------------------------------------------------
   Name - readCurStartTime
   Purpose - The operation returns the time (in micro seconds) at which the currently executing
			 process began its current time slice.
   Parameters - none
   Returns - The time (in micro seconds) at which the currently executing
			 process began its current time slice.
   Side Effects - none
   ------------------------------------------------------------------------ */
int readCurStartTime(){
    if(!inKernel()){
        if(DEBUG && debugInterrupts){
            USLOSS_Console("readCurStartTime: not in kernel mode\n");
        }
        USLOSS_Halt(1);
    }

    return Current->startTime;
}/* readCurStartTime */

/* ------------------------------------------------------------------------
   Name - timeSlice
   Purpose - The operation checks to see if the time slice has expired.  It will halt if
			 the process is in user mode.
   Parameters - none
   Returns - none
   Side Effects - May call dispatcher() if the total run time is under the time slice.
   ------------------------------------------------------------------------ */
void timeSlice(){
    // test if in kernel mode; halt if in user mode
    if(!inKernel()){
        if(DEBUG && debugInterrupts){
            USLOSS_Console("timeSlice: not in kernel mode\n");
        }
        USLOSS_Halt(1);
    }

    // gets the total run time of a proc and sees if it is under the TIMESLICE
    if(Current != NULL && Current->startTime != -1 && USLOSS_Clock() >= Current->startTime + TIMESLICE)
        dispatcher();
}/* timeSlice */

/* ------------------------------------------------------------------------
   Name - dispatcher
   Purpose - dispatches ready processes.  The process with the highest
             priority (the first on the ready list) is scheduled to
             run.  The old process is swapped out and the new process
             swapped in.
   Parameters - none
   Returns - nothing
   Side Effects - the context of the machine is changed
   ----------------------------------------------------------------------- */
void dispatcher(void)
{
    if(debugDispatcher && DEBUG)
        USLOSS_Console("dispatcher(): Beginning\n");
    
    if(!inKernel()){
        if(DEBUG && debugDispatcher){
            USLOSS_Console("dispatcher: not in kernel mode\n");
        }
        USLOSS_Halt(1);
    }
    
    disableInterrupts();
    
    if(debugDispatcher && DEBUG){
        USLOSS_Console("dispatcher(): Dump Process \n");
        dumpProcesses();
        printReadyList();
    }
    
    procPtr old = Current;
    Current = scheduler();    

    if(Current->status == READY){
        dequeueFromReadyList(Current);
    }
    
    // Checks to see if it had quit
    if(old == NO_CURRENT_PROCESS){
        old = NULL;
    }
    
    if(debugDispatcher && DEBUG){
        USLOSS_Console("dispatcher(): Current.name: %s    old.name: %s\n", Current->name, old == NULL ? "Nil" : old->name);
    }
    
    Current->status = RUNNING;
    
    USLOSS_ContextSwitch(old == NULL ? NULL : &old->state, &Current->state);
        
    p1_switch(old->pid, Current->pid);
} /* dispatcher */

/* ------------------------------------------------------------------------
   Name - scheduler
   Purpose - Prioritizes the processes in which order they should run based on their
			 priority values.  
   Parameters - none
   Returns - The process in the ready list to be run.
   Side Effects - If the current running process' is running and has expired its time slice,
				  call enqueueToReadyList(procPtr) with the current as the argument.
   ------------------------------------------------------------------------ */
procPtr scheduler(){
    int i;
    
    if(DEBUG && debugScheduler){
        USLOSS_Console("scheduler: in scheduler name: %s pid: %d\n", (Current == NO_CURRENT_PROCESS) ? "Nil" : Current->name, (Current == NO_CURRENT_PROCESS) ? -9999 : Current->pid);
    }
    
    for(i = 0; ReadyList[i] == NULL && i < SENTINELPRIORITY; i++){
        if(Current != NO_CURRENT_PROCESS){
            if(i + 1 == Current->priority){
                // If our current running process' is running and has expired its timeslice
                if(Current->status == RUNNING){
                    if (!(Current->startTime != -1 && USLOSS_Clock() > Current->startTime + Current->timeslice)){
                    	return Current;
                    }
                    // enqueue the running process back to ReadyList
                    enqueueToReadyList(Current);
                }
            }
        }
    }
    
    if(Current != NO_CURRENT_PROCESS && i < Current->priority){
        // first enqueue to list the process since we haven't yet
        if(Current->status == RUNNING)
            enqueueToReadyList(Current);
    }
    
        if(DEBUG && debugScheduler && Current != NULL){
            USLOSS_Console("scheduler: in scheduler name: %s pid: %d status %d\n", Current->name, Current->pid, Current->status);
        }

	ReadyList[i]->startTime = USLOSS_Clock();
	if(ReadyList[i]->sliceCount < 0)
		ReadyList[i]->sliceCount = 0;
	ReadyList[i]->sliceCount++;
    
    return ReadyList[i];
}/* scheduler */

/* ------------------------------------------------------------------------
   Name - sentinel
   Purpose - The purpose of the sentinel routine is two-fold.  One
             responsibility is to keep the system going when all other
             processes are blocked.  The other is to detect and report
             simple deadlock states.
   Parameters - none
   Returns - nothing
   Side Effects -  if system is in deadlock, print appropriate error
                   and halt.
   ----------------------------------------------------------------------- */
int sentinel (char *dummy)
{
    if (DEBUG && debugSentinel)
        USLOSS_Console("sentinel(): called\n");
    
    while (1)
    {
        checkDeadlock();
        USLOSS_WaitInt();
    }
} /* sentinel */

/* ------------------------------------------------------------------------
   Name - checkDeadlock
   Purpose - The operation checks to determine if a deadlock has occurred.
   Parameters - none
   Returns - none
   Side Effects - If there is a deadlock and we are debugging, call dumpProcesses()
				  and printReadyList(). USLOSS_Halt(0) will then be called.
   ------------------------------------------------------------------------ */
static void checkDeadlock()
{
    // test if in kernel mode; halt if in user mode
    if(!inKernel()){
        if(DEBUG && debugSentinel){
            USLOSS_Console("checkDeadlock: not in kernel mode\n");
        }
        USLOSS_Halt(1);
    }
    
    int i, procs = 0;
    for(i = 0; i < MAXPROC; i++){
        if(ProcTable[i].status != UNUSED)
            procs++;
    }
    
    if(procs > 1){
        if(DEBUG && debugSentinel){
            dumpProcesses();
            printReadyList();
        }
        USLOSS_Console("checkDeadlock(): numProc = %d. Only Sentinel should be left. Halting...\n", procs);
        USLOSS_Halt(1);
    }
    
    USLOSS_Console("All processes completed.\n");
    
    if(DEBUG && debugSentinel){
        dumpProcesses();
        printReadyList();
    }
    
    USLOSS_Halt(0);
} /* checkDeadlock */

 /* ------------------------------------------------------------------------
   Name - enableInterrupts
   Purpose - The operation enables interrupts.
   Parameters - none 
   Returns - none
   Side Effects - Turns on interrupts if we are in kernel mode.
   ------------------------------------------------------------------------ */
void enableInterrupts()
{
    int byte;
    // turn the interrupts ON iff we are in kernel mode
	// check to determine if we are in kernel mode
    if( (USLOSS_PSR_CURRENT_MODE | USLOSS_PsrGet()) == 0 ) {
        //not in kernel mode
        USLOSS_Console("Kernel Error: Not in kernel mode, may not ");
        USLOSS_Console("enable interrupts\n");
        USLOSS_Halt(1);
    }
    
    byte = USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT;
    // We ARE in kernel mode
    USLOSS_PsrSet(byte);
    if(DEBUG && debugInterrupts){
        USLOSS_Console("enable interrupts: byte: %d\n", byte);
    }
} /* enableInterrupts */


 /* ------------------------------------------------------------------------
   Name - disableInterrupts
   Purpose - The operation disables the interrupts.
   Parameters - none
   Returns - none
   Side Effects - Turns off interrupts if we are in kernel mode.
   ------------------------------------------------------------------------ */
void disableInterrupts()
{
    int byte;
    // turn the interrupts OFF iff we are in kernel mode
	// check to determine if we are in kernel mode
    if( (USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0 ) {
        //not in kernel mode
        USLOSS_Console("Kernel Error: Not in kernel mode, may not ");
        USLOSS_Console("disable interrupts\n");
        USLOSS_Halt(1);
    }
    
    byte = USLOSS_PsrGet() & ~USLOSS_PSR_CURRENT_INT;
    // We ARE in kernel mode
    USLOSS_PsrSet( byte );
    
    if(DEBUG && debugInterrupts){
        USLOSS_Console("disable interrupts: byte: %d\n", byte);
    }
} /* disableInterrupts */

/* ------------------------------------------------------------------------
   Name - clockHandler
   Purpose - The operation checks if the process has exceeded the time slice.
   Parameters - Deviation and arguments.
   Returns - none
   Side Effects - Halts the process if it is not in kernel mode. If the current
				  process is null but its status is running call timeSlice().
   ------------------------------------------------------------------------ */
void clockHandler(int dev, void *args){
	/*
	Note: - USLOSS_Clock returns a timeslice in microseconds
		  - calculates the time by taking current USLOSS time - starting time of the proc
	*/
	
    if(!inKernel()){
        if(DEBUG && debugInterrupts){
            USLOSS_Console("fork1: not in kernel mode\n");
        }
        USLOSS_Halt(1);
    }
    
    if(DEBUG && debugInterrupts)
        USLOSS_Console("clockHandler(): In clock handler\n");
	
	// If the current process is null and its status is running call timeSlice()
    if(Current != NULL && Current->status == RUNNING)
        timeSlice();
}/* clockHandler */

/* ------------------------------------------------------------------------
   Name - inKernel
   Purpose - Gets the right most bit from the current process.
   Parameters - none
   Returns - The right most bit of the current process.
   Side Effects - none
   ------------------------------------------------------------------------ */
int inKernel(){
    return (USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet());
}/* inKernel */
    
/* ------------------------------------------------------------------------
   Name - readtime
   Purpose - Gets the CPU time used by the current process in ms.
   Parameters - none
   Returns - The time of USLOSS in ms.
   Side Effects - Calls USLOSS_Halt(1) if it is not in kernel mode.
   ------------------------------------------------------------------------ */
int readtime(){
    // test if in kernel mode; halt if in user mode
    if(!inKernel()){
        if(DEBUG && debugInterrupts){
            USLOSS_Console("readtime: not in kernel mode\n");
        }
        USLOSS_Halt(1);
    }
    if(Current == NULL){
	return -1;	
    }

    return (USLOSS_Clock() / Current->startTime) / 1000;
}/* readTime */

/* ------------------------------------------------------------------------
   Name - cleanTableSlot
   Purpose - Cleans a slot in the process table by clearing the values.
   Parameters - The slot in the process table to be cleaned.
   Returns - none
   Side Effects - All fields in the process indicated by the pid will be cleaned out.
   ------------------------------------------------------------------------ */
void cleanProcData(int slot){
    if(DEBUG && debugCleanTableEntry)
        USLOSS_Console("cleanData(): %d\n", ProcTable[slot].pid);
    strcpy(ProcTable[slot].startArg, "Nil");
    ProcTable[slot].startFunc = NULL;
    ProcTable[slot].childProcPtr = NULL;
    ProcTable[slot].nextDeadChild = NULL;
    ProcTable[slot].nextProcPtr = NULL;
    ProcTable[slot].stack = NULL;
    ProcTable[slot].isZapped = FALSE;
    ProcTable[slot].numActiveChildren = 0;
    ProcTable[slot].numChildren = 0;
    ProcTable[slot].numDeadChildren = 0;
    ProcTable[slot].numZombieChildren = 0;
    ProcTable[slot].numJoinedChildren = 0;
    ProcTable[slot].stackSize = 0;
}/* cleanTableSlot */

void cleanTableEntry(int slot){
    if(DEBUG && debugCleanTableEntry){
        USLOSS_Console("cleanEntry(): %d\n", ProcTable[slot].pid);
    	dumpProcesses();
    }
    strcpy(ProcTable[slot].name, "Nil");
    if(ProcTable[slot].parentPtr != NULL){
	ProcTable[slot].parentPtr->tableChildren--;
    }
    ProcTable[slot].parentPtr = NULL;
    ProcTable[slot].pid = -1;
    ProcTable[slot].priority = -1;
    ProcTable[slot].timeslice = -1;
    ProcTable[slot].startTime = -1;
    ProcTable[slot].sliceCount = -1;
    ProcTable[slot].deadStatus = -9999;
    ProcTable[slot].tableSlot = -1;
    ProcTable[slot].ppid = -1;
    ProcTable[slot].status = UNUSED;
    if(DEBUG && debugCleanTableEntry){
    	dumpProcesses();
    }
}

/* ------------------------------------------------------------------------
   Name - enqueueToReadyList
   Purpose - Enqueues a running process to the ready list.
   Parameters - The running process to be enqueued to the ready list.
   Returns - none
   Side Effects - Puts the process onto the ready list and gives it a priority and sets
				  the status to READY.
   ------------------------------------------------------------------------ */
void enqueueToReadyList(procPtr ptr){
    procPtr cur = NULL;
    ptr->nextProcPtr = NULL;
    if(ptr != NULL){
        if(ReadyList[ptr->priority - 1] == NULL){
            ReadyList[ptr->priority - 1] = ptr;
        }
        else{
            for(cur = ReadyList[ptr->priority - 1]; cur->nextProcPtr != NULL; cur = cur->nextProcPtr);
            cur->nextProcPtr = ptr;
        }
        ptr->status = READY;
	ptr->startTime = -1;
    }
    if(DEBUG && debugQueue){
        USLOSS_Console("Enqueue %s to list\n", ptr->name);
        printReadyList();
    }
}/* enqueueToReadyList */

/* ------------------------------------------------------------------------
   Name - dequeueFromReadyList
   Purpose - Dequeue a process from the ready list
   Parameters - The next process to be dequeued from the ready list.
   Returns - none
   Side Effects -  The process is dequeued from the ready list.
   ------------------------------------------------------------------------ */
void dequeueFromReadyList(procPtr nextProcess){
    procPtr cur = NULL;
    cur = ReadyList[nextProcess->priority - 1];
    if(cur != NULL){
        ReadyList[cur->priority - 1] = ReadyList[cur->priority - 1]->nextProcPtr;
        cur->nextProcPtr = NULL;
    }
    if(DEBUG && debugQueue){
        USLOSS_Console("Dequeue %s from list\n", nextProcess->name);
        printReadyList();
    }
}/* dequeueFromReadyList */
