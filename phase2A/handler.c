#include <stdio.h>
#include <phase1.h>
#include <phase2.h>
#include "message.h"

int debugNullsys = 0;
int debugClock = 0;
int debugDisk = 0;
int debugTerm = 0;
int debugSyscall = 0;

int interruptCounter = 0;

// system call vector
extern void (*systemCallVec[])(systemArgs *);

extern int readCurStartTime(void);


/* ------------------------------------------------------------------------
   Name - nullsys
   Purpose - halts the simulator for invalid syscalls
   Parameters - systemArgs struct of syscall id and args
   Returns - none
   Side Effects - error message prints to screen and halts
   ----------------------------------------------------------------------- */
void nullsys(systemArgs *args)
{
    checkKernelMode("nullsys");
    USLOSS_Console("nullsys(): Invalid syscall %d. Halting...\n", args->number);
    USLOSS_Halt(1);
} /* nullsys */

/* ------------------------------------------------------------------------
   Name - clockHandler2
   Purpose - handles clock interrupts on every 5th interrupt, calls timeslice function
   Parameters - device id, argument for clock interrupt, essentially useless
   Returns - none
   Side Effects - every fifth interrupt will call dispatcher
		  unblocks any proc blocked on clock mailbox
   ----------------------------------------------------------------------- */
void clockHandler2(int dev, void *arg)
{

   checkKernelMode("clockHandler2");
    disableInterrupts();
   if(DEBUG2 && debugClock)
     USLOSS_Console("clockHandler2(): called\n");
	
	// error check, is the device actually a clock device?
	if(dev != USLOSS_CLOCK_INT){
        if(DEBUG2 && debugClock){
            USLOSS_Console("clockHandler(): dev is %d, which is not a clock device!", dev);
        }
		USLOSS_Halt(1);
	}
	
	// conditionally send to clock I/O mailbox every 5th clock interrupt	
	
	// call readCurStartTime, check since then (USLOSS_Clock), call time slice; every 5th interrupt send
	int startTime = readCurStartTime();
	int currTime = USLOSS_Clock();
	int nowTime = currTime - startTime;
	if(nowTime >= 80000){
		timeSlice();
		interruptCounter++;
	}

	if(interruptCounter % 5 == 0){
		MboxCondSend(0, "\0\0\0\0", sizeof(int));
	}

} /* clockHandler */

/* ------------------------------------------------------------------------
   Name - diskHandler
   Purpose - handles the disk interrupt, sending disk message to receiver
   Parameters - device id, argument for disk interrupt, this is the unit number 
		that signifies which disk driver
   Returns - none
   Side Effects - will unblock a process blocked on that disk's mailbox
		  will halt the simulator if arg is none of the disk units
		  will halt the simulator if dev is not the disk 
   ----------------------------------------------------------------------- */
void diskHandler(int dev, void *arg)
{

    checkKernelMode("diskHandler");
    if(DEBUG2 && debugDisk)
      USLOSS_Console("diskHandler(): called\n");

	// error check, is the device actually a disk device?
	if(dev != USLOSS_DISK_INT){
        if(DEBUG2 && debugDisk){
            USLOSS_Console("diskHandler(): dev is %d, which is not a disk device!", dev);
        }
		USLOSS_Halt(1);
	}
	
	// is the unit number in the correct range?
	int unit_number = (long) arg;
    
    // assign the mbox
    int mboxId = -1;
    switch (unit_number) {
        case 0:
            mboxId = DISKBOX2;
            break;
        case 1:
            mboxId = DISKBOX1;
            break;
            
        default:
            if(DEBUG2 && debugDisk){
                USLOSS_Console("diskHandler(): the unit number is [%d], outside the range", unit_number);
            }
            USLOSS_Halt(1);
    }
    
    if(DEBUG2 && debugDisk){
        USLOSS_Console("diskHandler: unit number %d\n", unit_number);
    }
	
	// int USLOSS_DeviceInput(int dev, int unit, int &status);
    int status;
	USLOSS_DeviceInput(dev, unit_number, &status);
    
    // conditionally send contents of status register to appropriate mailbox
    MboxCondSend(mboxId, &status, sizeof(int));

} /* diskHandler */

/* ------------------------------------------------------------------------
   Name - termHandler
   Purpose - handles the terminal interrupt, sending terminal message to receiver
   Parameters - device id, argument for terminal interrupt, this is a unit that
		signifies which of 4 terminal drivers
   Returns - none
   Side Effects - will unblock a process blocked on that terminal mailbox
		  will halt the simulator if arg is none of the terminal units
		  will halt the simulator if dev is not the terminal
   ----------------------------------------------------------------------- */
void termHandler(int dev, void *arg)
{

    checkKernelMode("termHandler");
    if(DEBUG2 && debugTerm)
      USLOSS_Console("termHandler(): called\n");

    // error check, is the device actually a term device?
	if(dev != USLOSS_TERM_INT){
        if(DEBUG2 && debugTerm){
            USLOSS_Console("termHandler(): dev is %d, which is not a term device!", dev);
        }
		USLOSS_Halt(1);
	}
	
	// is the unit number in the correct range?
	int unit_number = (long) arg;
    int status;
    
    // assign the mbox
    int mboxId = -1;
    switch (unit_number) {
        case 0:
            mboxId = TERMBOX1;
            break;
        case 1:
            mboxId = TERMBOX2;
            break;
        case 2:
            mboxId = TERMBOX3;
            break;
        case 3:
            mboxId = TERMBOX4;
            break;
            
        default:
            if(DEBUG2 && debugTerm){
                USLOSS_Console("termHandler(): the unit number is [%d], outside the range", unit_number);
            }
            USLOSS_Halt(1);
    }
    
    if(DEBUG2 && debugTerm){
        USLOSS_Console("termHandler: unit number %d\n", unit_number);
    }
    
    // int USLOSS_DeviceInput(int dev, int unit, int &status);
    USLOSS_DeviceInput(dev, unit_number, &status);
    
	// conditionally send contents of status register to appropriate mailbox
	MboxCondSend(mboxId, &status, sizeof(int));
	
} /* termHandler */


/* ------------------------------------------------------------------------
   Name - syscallHandler
   Purpose - handles system call interrupts by executing the syscall
   Parameters - device id, systemArgs* object with sycall parameters
   Returns - none
   Side Effects - executes syscall function
		  will halt the simulator if dev is not the syscall device
		  systemCallVec indices are initialized to nullsys so will halt
		  if the index of the systemArgs is not changed to another function
   ----------------------------------------------------------------------- */
void syscallHandler(int dev, void *arg)
{
    checkKernelMode("syscallHandler");
    
    if(DEBUG2 && debugSyscall)
      USLOSS_Console("syscallHandler(): called\n");
    
    // error check, is the device actually a syscall device?
	if(dev != USLOSS_SYSCALL_INT){
        if(DEBUG2 && debugSyscall){
            USLOSS_Console("syscallHandler(): dev is %d, which is not a syscall device!", dev);
        }
		USLOSS_Halt(1);
	}

    // set void * to systemArgs *
    systemArgs *args = (systemArgs*) arg;
    
    if(DEBUG2 && debugSyscall){
        USLOSS_Console("syscallHandler %d\n", args->number);
    }
    
    // error check, is the device actually a syscall device?
    if(args->number < 0 || args->number >= MAXSYSCALLS){
        if(DEBUG2 && debugSyscall){
            USLOSS_Console("syscallHandler(): syscall %d out of range", args->number);
        }
        USLOSS_Console("syscallHandler(): sys number %d is wrong.  Halting...\n", args->number);
        USLOSS_Halt(1);
    }
    
    // call function pointed to by systemArgs passing in systemArgs *
    // Should be nullsys for now
    systemCallVec[args->number](args);
	
} /* syscallHandler */
