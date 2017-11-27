//
//  Clock.c
//  phase4
//
//  Created by Sean Vaeth on 11/10/16.
//  Copyright © 2016 Sean Vaeth. All rights reserved.
//

#include <usloss.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <stdlib.h> /* needed for atoi() */

#include "providedDependencies.h"
#include "providedPrototypes.h"
#include "clock.h"

// ====================PROTOTYPES=======================
extern void checkKernel(char *);
void insertClockReq(procPtr);
void dequeueClockReq(void);
void printClockList(void);

// ====================GLOBALS==========================
extern procStruct Proc4Table[];
extern semaphore running;

procPtr ClockList = NULL;

// ====================DEBUG FLAGS==========================
int debugClock = 0;
int debugSleep = 0;
static int debugQueue = 0;

/*
 *  Routine:  ClockDriver
 *
 *  Description: 
 *
 *  Arguments: args -- the unit of the disk.
 *
 *  Return: none
 *  side effect: 
 */
int ClockDriver(char *arg) {
    int result;
    int status;
    procPtr cur;
    
    // Let the parent know we are running and enable interrupts.
    semvReal(running.semID);
    USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);
    
    // Infinite loop until we are zap'd
    while(! isZapped()) {
        result = waitDevice(USLOSS_CLOCK_DEV, 0, &status);
        
        if (result != 0) {
            return 0;
        }
        
        /*
         * Compute the current time and wake up any processes
         * whose time has come.
         */
        cur = ClockList;
        int curTime = USLOSS_Clock();
        while(cur != NULL && cur->wakeupTime <= curTime){

            if(DEBUG4 && debugClock){
                USLOSS_Console("CUR %d WAKEUP %d\n", curTime, cur->wakeupTime);
            }
            
            // dequeue from clock driver queue
            dequeueClockReq();
            
            // unblock request
            semvReal(cur->sem.semID);
            
            
            //update cur
            cur = ClockList;
        }
    }
    
    return 0;
}/* ClockDriver */

/*
 *  Routine:  sleep4
 *
 *  Description: 
 *
 *  Arguments: args -- a system argument struct containing the seconds to be used in
 *					   sleepReal
 *
 *  Return: none
 *  side effect: 
 */
void sleep4(systemArgs *args){
    checkKernel("sleep4");
    
    int toSleep = (int) (long) args->arg1;
    
    sleepReal(toSleep);
    
}/* sleep4 */

/*********		CLOCK DRIVER FUNCTIONS		*********/
/*
 *  Routine:  sleepReal
 *
 *  Description: Causes the calling process to become unrunnable
 *				 for at least the specified number ofseconds, and
 *				 not significantly longer. The seconds must be non-negative.
 *
 *  Arguments: seconds -- the amount of seconds for the calling process to be unrunnable
 *
 *  Return: -1 if the seconds are negative, 0 if otherwise
 *  side effect: none
 */
int sleepReal(int seconds){
    procPtr cur = &Proc4Table[getpid() % MAXPROC];
    
    if(seconds < 0){
        return -1;
    }
    
    // set pid, calculate wakeup time
    cur->pid = getpid();
    cur->wakeupTime = USLOSS_Clock() + seconds * MICROS;
    
    //insert to clock driver list
    insertClockReq(cur);
    
    //block process
    sempReal(cur->sem.semID);
    
    return 0;
}/* sleepReal */

/*
 *  Routine:  insertClockReq
 *
 *  Description: Inserts a process to the clock list
 *
 *  Arguments: ptr -- the procPtr to be inserted into the clock list.
 *
 *  Return: none
 *  side effect: 
 */
void insertClockReq(procPtr ptr){
    procPtr cur = NULL;
    if(ptr != NULL){
        if(ClockList == NULL){
            ClockList = ptr;
        }
        else if(ptr->wakeupTime < ClockList->wakeupTime){
            ptr->nextProcPtr = ClockList;
            ClockList = ptr;
        }
        else{
            for(cur = ClockList; cur->nextProcPtr != NULL && cur->nextProcPtr->wakeupTime < cur->wakeupTime; cur = cur->nextProcPtr);
            ptr->nextProcPtr = cur->nextProcPtr;
            cur->nextProcPtr = ptr;
        }
    }
    if(DEBUG4 && debugQueue){
        USLOSS_Console(cur == NULL ? "NULL\n" : "Enqueue %d to list\n", ptr->pid);
        printClockList();
    }
}/* insertClockReq */


 /*
 *  Routine:  dequeueClockReq
 *
 *  Description: Dequeue a process from the clock list
 *
 *  Arguments: none
 *
 *  Return: none
 *  side effect: The process is dequeued from the ready list.
 */
void dequeueClockReq(){
    procPtr cur = ClockList;
    if(cur != NULL){
        ClockList = ClockList->nextProcPtr;
        cur->nextProcPtr = NULL;
    }
    if(DEBUG4 && debugQueue){
        USLOSS_Console( cur == NULL ? "NULL\n" : "Dequeue %d from list\n", cur->pid);
        printClockList();
    }
}/* dequeueClockReq */

 /*
 *  Routine:  printClockList
 *
 *  Description: Used for debugging to print out the clock list.
 *
 *  Arguments: none
 *
 *  Return: none
 *  side effect: none
 */
void printClockList(){
    USLOSS_Console("********* CLOCK LIST **********\n");
    procPtr cur = ClockList;
        USLOSS_Console("Clock: ");
        while(cur != NULL){
            USLOSS_Console("|%d|", cur->pid);
            if(cur->nextProcPtr != NULL)
                USLOSS_Console("->");
            cur = cur -> nextProcPtr;
        }
        USLOSS_Console("NULL\n");
    
    USLOSS_Console("********* CLOCK LIST **********\n");
}/* printClockList */


