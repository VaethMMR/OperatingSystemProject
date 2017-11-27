#include <usloss.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <usyscall.h>
#include "clock.h"
#include "disk.h"
#include "term.h"
#include "providedPrototypes.h"
#include "providedDependencies.h"
#include <stdio.h>
#include "string.h"



// ====================GLOBALS==========================

extern void (*systemCallVec[MAXSYSCALLS])(systemArgs*);
extern semaphore running;
procStruct Proc4Table[MAXPROC];
semaphore diskLock;

// ====================DEBUG FLAGS==========================
int debugKernel = 0;
int debugClockDriver = 0;
int debugDiskDriver = 0;
int debugTermDriver = 0;
int debugTermReader = 0;
int debugTermWriter = 0;

// ====================PROTOTYPES=======================

extern int start4(char *);

void checkKernel(char *);
void enqueueDeviceRequest(procPtr, int);
void dequeueDeviceRequest(int);
void printDeviceQueues(void);

/* ------------------------------------------------------------------------
 Name - start3
 Purpose - Initialize phase 4 system calls and device drivers
 Parameters - none
 Returns - none
 Side Effects - none
 ------------------------------------------------------------------------ */
void start3(void) {
    char	name[128];
    char    termbuf[10];
    char    buf[512];
    int		i;
    int		clockPID;
    int     diskPIDS[USLOSS_DISK_UNITS];
    int     termPIDS[USLOSS_TERM_UNITS];
    int     termReadPIDS[USLOSS_TERM_UNITS];
    int     termWritePIDS[USLOSS_TERM_UNITS];
    int		pid;
    int		status;
    FILE    *fp;
    char    file[20];

    // emtpy buffers
    for(i = 0; i < 512; i++){
        if(i < 10){
            termbuf[i] = '\0';
        }
        if(i < 128){
            name[i] = '\0';
        }
        buf[i] = '\0';
    }
    
    //ininitalize semaphores
    for(i = 0; i < MAXPROC; i++){
        Proc4Table[i].sem.semID = semCreateReal(0);
    }
	
    /*
     * Check kernel mode here.
     */
	 checkKernel("start3");
	
    // initialize system calls to the systemCallVec
    systemCallVec[SYS_SLEEP] = sleep4;
    systemCallVec[SYS_DISKREAD] = diskRead;
    systemCallVec[SYS_DISKWRITE] = diskWrite;
    systemCallVec[SYS_DISKSIZE] = diskSize;
    systemCallVec[SYS_TERMREAD] = termRead;
    systemCallVec[SYS_TERMWRITE] = termWrite;
    
	/*
     * Data structure initialization as needed...
     */
    
    /*
     * Create clock device driver 
     * I am assuming a semaphore here for coordination.  A mailbox can
     * be used instead -- your choice.
     */
    running.semID = semCreateReal(0);
    diskLock.semID = semCreateReal(1);
    clockPID = fork1("Clock driver", ClockDriver, NULL, USLOSS_MIN_STACK, 2);
    if (clockPID < 0) {
        USLOSS_Console("start3(): Can't create clock driver\n");
        USLOSS_Halt(1);
    }
    
    if(DEBUG4 && debugClockDriver){
        USLOSS_Console("ClockPID %d\n", clockPID);
    }
    
    /*
     * Wait for the clock driver to start. The idea is that ClockDriver
     * will V the semaphore "running" once it is running.
     */

    semPReal(running.semID);

    /*
     * Create the disk device drivers here.  You may need to increase
     * the stack size depending on the complexity of your
     * driver, and perhaps do something with the pid returned.
     */

    for (i = 0; i < USLOSS_DISK_UNITS; i++) {
        sprintf(name, "Disk Driver %d", i);
        sprintf(buf, "%d", i);
        pid = fork1(name, DiskDriver, buf, USLOSS_MIN_STACK, 2);
        if (pid < 0) {
            USLOSS_Console("start3(): Can't create disk driver %d\n", i);
            USLOSS_Halt(1);
        }
        diskPIDS[i] = pid;
        
        if(DEBUG4 && debugDiskDriver){
            USLOSS_Console("DiskPID %d: %d\n", i, pid);
        }
        
        /*
         * Wait for the disk driver to start. The idea is that DiskDriver
         * will V the semaphore "running" once it is running.
         */
        
        semPReal(running.semID);
    }

    // May be other stuff to do here before going on to terminal drivers

    /*
     * Create terminal device drivers.
     */
    
    for (i = 0; i < USLOSS_TERM_UNITS; i++) {
        sprintf(name, "Term Driver %d", i);
        sprintf(buf, "%d", i);
        pid = fork1(name, TermDriver, buf, USLOSS_MIN_STACK, 2);
        if (pid < 0) {
            USLOSS_Console("start3(): Can't create term driver %d\n", i);
            USLOSS_Halt(1);
        }
        termPIDS[i] = pid;
        
        if(DEBUG4 && debugTermDriver){
            USLOSS_Console("TermPID %d: %d\n", i, pid);
        }
        
        /*
         * Wait for the terminal driver to start. The idea is that TermDriver
         * will V the semaphore "running" once it is running.
         */
        
        semPReal(running.semID);
    }

    /*
     * Create terminal device readers.
     */
    
    for (i = 0; i < USLOSS_TERM_UNITS; i++) {
        sprintf(name, "Term Reader %d", i);
        sprintf(buf, "%d", i);
        pid = fork1(name, TermReader, buf, USLOSS_MIN_STACK, 2);
        if (pid < 0) {
            USLOSS_Console("start3(): Can't create term driver %d\n", i);
            USLOSS_Halt(1);
        }
        
        termReadPIDS[i] = pid;
        
        
        if(DEBUG4 && debugTermReader){
            USLOSS_Console("TermReadPID %d: %d\n", i, pid);
        }
        
        /*
         * Wait for the terminal driver to start. The idea is that TermDriver
         * will V the semaphore "running" once it is running.
         */
        
        semPReal(running.semID);
    }
//
//    /*
//     * Create terminal device writers.
//     */
//    
    for (i = 0; i < USLOSS_TERM_UNITS; i++) {
        sprintf(name, "Term Writer %d", i);
        sprintf(buf, "%d", i);
        pid = fork1(name, TermWriter, buf, USLOSS_MIN_STACK, 2);
        if (pid < 0) {
            USLOSS_Console("start3(): Can't create term driver %d\n", i);
            USLOSS_Halt(1);
        }
        termWritePIDS[i] = pid;
        
        if(DEBUG4 && debugTermWriter){
            USLOSS_Console("TermWritePID %d: %d\n", i, pid);
        }
        
        /*
         * Wait for the terminal driver to start. The idea is that TermDriver
         * will V the semaphore "running" once it is running.
         */
        
        semPReal(running.semID);
    }

    // set up relations of terminal devices
    for(i = 0; i < USLOSS_TERM_UNITS; i++){
        // set drivers' readers/writers
        Proc4Table[termPIDS[i] % MAXPROC].reader = &Proc4Table[termReadPIDS[i] % MAXPROC];
        Proc4Table[termPIDS[i] % MAXPROC].writer = &Proc4Table[termWritePIDS[i] % MAXPROC];
        
        // set readers' drivers/writers
        Proc4Table[termReadPIDS[i] % MAXPROC].driver = &Proc4Table[termPIDS[i] % MAXPROC];
        Proc4Table[termReadPIDS[i] % MAXPROC].writer = &Proc4Table[termWritePIDS[i] % MAXPROC];
        
        // set writers' drivers/readers
        Proc4Table[termWritePIDS[i] % MAXPROC].driver = &Proc4Table[termPIDS[i] % MAXPROC];
        Proc4Table[termWritePIDS[i] % MAXPROC].reader = &Proc4Table[termReadPIDS[i] % MAXPROC];
    }

    /*
     * Create first user-level process and wait for it to finish.
     * These are lower-case because they are not system calls;
     * system calls cannot be invoked from kernel mode.
     * I'm assuming kernel-mode versions of the system calls
     * with lower-case first letters, as shown in provided_prototypes.h
     */
    pid = spawnReal("start4", start4, NULL, 4 * USLOSS_MIN_STACK, 3);
    pid = waitReal(&status);

    /*
     * Zap the device drivers
     */
    
    if(DEBUG4 && debugClockDriver){
        USLOSS_Console("Zap ClockPID %d\n", clockPID);
    }
    
    zap(clockPID);  // clock driver
    
    // disk drivers
    for(i = 0; i < USLOSS_DISK_UNITS; i++){
        if(DEBUG4 && debugDiskDriver){
            USLOSS_Console("Zap DiskPID %d: %d\n", i, diskPIDS[i]);
        }
        semVReal(Proc4Table[diskPIDS[i] % MAXPROC].sem.semID);
        zap(diskPIDS[i]);
    }
    
    // term drivers
    for(i = 0; i < USLOSS_TERM_UNITS; i++){
        if(DEBUG4 && debugTermDriver){
            USLOSS_Console("Zap TermPID %d: %d\n", i, termPIDS[i]);
        }
        sprintf(file, "term%d.in", i);
        fp = fopen(file, "a");
        fprintf(fp, "This is the end of file string\n");
        fclose(fp);
        zap(termPIDS[i]);
    }
    
    // term readers
    for(i = 0; i < USLOSS_TERM_UNITS; i++){
        if(DEBUG4 && debugTermReader){
            USLOSS_Console("Zap TermReadPID %d: %d\n", i, termReadPIDS[i]);
        }
        MboxCondSend(Proc4Table[termReadPIDS[i] % MAXPROC].readerMbox, NULL, 0);
        zap(termReadPIDS[i]);
    }
    
    // term writers
    for(i = 0; i < USLOSS_TERM_UNITS; i++){
        if(DEBUG4 && debugTermWriter){
            USLOSS_Console("Zap TermWritePID %d: %d\n", i, termWritePIDS[i]);
        }
        MboxCondSend(Proc4Table[termWritePIDS[i] % MAXPROC].writerMbox, NULL, 0);
        zap(termWritePIDS[i]);
    }

    // eventually, at the end:
    quit(0);
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
    if(DEBUG4 && debugKernel){
        USLOSS_Console("checkKernelMode: from %s byte: %d\n", name, byte);
    }
    if(byte != 1){
        if(DEBUG4 && debugKernel){
            USLOSS_Console("%s: not in kernel mode\n", name);
        }
        USLOSS_Halt(1);
    }
}