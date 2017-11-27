//
//  Term.c
//  phase4
//
//  Created by Sean Vaeth on 11/10/16.
//  Copyright © 2016 Sean Vaeth, Brandon Wong. All rights reserved.
//

#include <usloss.h>
#include <phase1.h>
#include <phase2.h>
#include <stdlib.h> /* needed for atoi() */

#include "term.h"
#include "string.h"
#include "providedPrototypes.h"
#include "providedDependencies.h"

// ====================PROTOTYPES=======================

extern int TermDriver(char *);
extern int TermReader(char *);
extern int TermWriter(char *);
extern void checkKernel(char *);

// ====================GLOBALS==========================
extern procStruct Proc4Table[];
extern semaphore running;

// ====================DEBUG FLAGS==========================
int debugTerm = 0;
int debugTermRead = 0;
int debugTermWrite = 0;

int TermUnits[USLOSS_TERM_UNITS];
int TermCtrlReg;
 
/*
 *  Routine:  TermDriver
 *
 *  Description: Driver for four instances.  waitDevice() will be called
 *				 and depending on the result of waitDevice() depends on the
 *				 reciept of a character or completion of the send of a character
 *				 or both. Recieved characters are sent to a mailbox and the result
 *				 of output of output characters are sent to a mailbox.
 *
 *  Arguments: args -- the unit of the disk.
 *
 *  Return: none
 *  side effect: determines if the transfer was successful and if illegal values were given
 *				 as input.
 */
int TermDriver(char *arg){
    int result;
    int status = 0;
    int unit;
    char ch;
    procPtr cur = &Proc4Table[getpid() % MAXPROC];
    
    unit = atoi(arg);
    TermUnits[unit] = getpid();
    
    //enable recv interrupts
    USLOSS_TERM_CTRL_RECV_INT(status);
    USLOSS_DeviceOutput(USLOSS_TERM_DEV, unit, &status);
    
    semVReal(running.semID);
    USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);
    
    // store the control regs
    TermCtrlReg = status;
    
    // Infinite loop until we are zap'd
    while(! isZapped()) {
        // wait device
        result = waitDevice(USLOSS_TERM_DEV, unit, &status);
        if(result != 0){
            return 0;
        }
        
        //write character to reader if there is a char to receive
        if(USLOSS_TERM_STAT_RECV(status) ==  USLOSS_DEV_BUSY)
            MboxCondSend(cur->reader->readerMbox, &ch, 1);
        
        //write character to writer if there is char to xmit
        if(USLOSS_TERM_STAT_XMIT(status) == USLOSS_DEV_READY)
            MboxCondSend(cur->writer->writerMbox, NULL, 0);
        
        if(isZapped()){
            return 0;
        }
    }
    return 0;
}/* TermDriver */

/*
 *  Routine:  TermReader
 *
 *  Description: reader for four instances.  Individual characters from a mailbox and
 *				 builds "lines", delimited by newlines, or when MAXLINE characters have
 *				 been read.  Then it sends the completed lines to a mailbox.  It can
 *				 buffer up to 10 lines, and discards lines (not characters) when limit is reached.
 *
 *  Arguments: args -- the unit of the disk.
 *
 *  Return: none
 *  side effect: 
 */
int TermReader(char *arg){
    procPtr cur = &Proc4Table[getpid() % MAXPROC];
    char buf[MAXLINE];
    int unit;
    int i;
    
    // initialize mailboxes
    cur->readerMbox = MboxCreate(0, sizeof(int));
    cur->receiverMbox = MboxCreate(10, MAXLINE * sizeof(char));

    unit = atoi(arg);
    for(i = 0; i < MAXLINE; i++){
        buf[i] = '\0';
    }
    
    semVReal(running.semID);
    USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);
    
    i = 0;
    // Infinite loop until we are zap'd
    while(! isZapped()) {
        int msg;
        
        // wait for input to read
        MboxReceive(cur->readerMbox, &msg, sizeof(int));
        
        // collect characters from character-in mbox
        buf[i++] = (char) msg;
        
        //build lines with newlines or MAXLINE
        if(msg == '\n' || i == MAXLINE){
            //send completed lines to mailbox.
            //box only holds 10 slots, 1 per line
            MboxCondSend(cur->receiverMbox, buf, sizeof(char) * i);
            
            for(i = 0; i < MAXLINE; i++){
                buf[i] = '\0';
            }
            i = 0;
        }
    }
    return 0;
}/* TermReader */

/*
 *  Routine:  TermWriter
 *
 *  Description: writer for four instances.  Recieves a line of output from mbox or semaphore protected structure
 *				 and sets the terminal to transmit interrupts enabled.  On each interrupt,
 *				 that indicates DEV_READY, send one character.  When the string is done,
 *				 disable transmit interrupts and send the results to the user process via
 *				 priavte mailbox or structure procted by private semaphore.
 *
 *  Arguments: args -- the unit of the disk.
 *
 *  Return: none
 *  side effect: 
 */
int TermWriter(char *arg){
    procPtr cur = &Proc4Table[getpid() % MAXPROC];
    int ctrl, status, len;
    char msg[MAXLINE], i, ch;
    int unit = atoi(arg);
    
    semVReal(running.semID);
    USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);
    
    cur->writerMbox = MboxCreate(0, MAXLINE);
    
    // Infinite loop until we are zap'd
    while(! isZapped()) {
        // Receive a line of output from mbox
        MboxReceive(cur->writerMbox, msg, MAXLINE);
        
        len = (int) strlen(msg);
        //enable terminal xmit interrupts
        ctrl = TermCtrlReg;
        USLOSS_TERM_CTRL_XMIT_INT(ctrl);
        
        //for each xmit DEV_READY send one char
        for(i = 0; i < len; i++){
            USLOSS_DeviceInput(USLOSS_TERM_DEV, unit, &status);
            USLOSS_TERM_STAT_XMIT(status);
            if(status == USLOSS_DEV_READY){
                
                // set character to send
                ch = msg[i];
                USLOSS_TERM_CTRL_CHAR(ctrl, ch);
                
                // set send char bit
                
                USLOSS_TERM_CTRL_XMIT_CHAR(ctrl);

                // send
                USLOSS_DeviceOutput(USLOSS_TERM_DEV, unit, &ctrl);
            }
        }
        
        // wait device to return value
        waitDevice(USLOSS_TERM_DEV, unit, &status);
        
        // disable xmit interrupts
        ctrl = ctrl & ~0x4;
        
        //send result to user process via private mbox
        MboxCondSend(Proc4Table[TermUnits[unit] % MAXPROC].writerMbox, &status, MAXLINE);
    }
    
    return 0;
}/* TermWriter*/

/*********		TERMINAL DRIVER FUNCTIONS		*********/

/*
 *  Routine:  termRead
 *
 *  Description: gets the arguments from the system argument to be used in termReadReal
 *
 *  Arguments: args -- a system argument struct containing information to be used in
 *					   termReadReal.
 *
 *  Return: none
 *  side effect: 
 */
void termRead (systemArgs *args){
    char *bufferAddress = (char*) args->arg1;
    int sizeNum = (int) (long) args->arg2;
    int unitNum = (int) (long) args->arg3;
    
    args->arg2 = (void*) (long) termReadReal(unitNum, sizeNum, bufferAddress);
    if(args->arg2 == (void*) (long) -1){
       args->arg4 = (void*) (long) -1;
    }
    else{
	args->arg4 = 0;
    }
}/* termRead */

/*
 *  Routine:  termReadReal
 *
 *  Description: reads a line of text from the terminal indicated by 'unit' into the buffer
 *				 pointed to by 'buffer'.  A line is terminated by a newline character ('\n') which
 *				 is copied into the buffer along with the other characters in the line.  If the
 *				 length of a line of input is greater than the value of the 'size' parameter, then the
 *				 first 'size' characters are returned and the rest are discarded.
 *
 *  Arguments: unit -- the unit of the disk.
 *			   size -- the size allowed to be read.
 *			   buffer -- the buffer to be read from.
 *
 *  Return: none
 *  side effect: 
 */
int termReadReal(int unit, int size, char *buffer){
   
    if(unit < 0 || unit > USLOSS_TERM_UNITS || size < 0){
         return -1;
    }    
 
    // receive line of input from mailbox associated with indicated terminal
    MboxReceive(TermUnits[unit], buffer, size);
    int chars = (int) strlen(buffer);
    
    return chars;
}/* termReadReal */


/*
 *  Routine:  termWrite
 *
 *  Description: gets the arguments from the system argument to be used in termWriteReal
 *
 *  Arguments: args -- a system argument struct containing information to be used in
 *					   termWriteReal
 *
 *  Return: none
 *  side effect: 
 */
void termWrite(systemArgs *args){
    int chars;
    char *bufferAddress = (char*) args->arg1;
    int bufferSizeNum = (int) (long) args->arg2;
    int unitNum = (int) (long) args->arg3;
    
    args->arg2 = (void *) (long) termWriteReal(bufferAddress, bufferSizeNum, unitNum, &chars);
    if(args->arg2 == (void*) (long) -1){
       args->arg4 = (void*) (long) -1;
    }
    else{
        args->arg4 = 0;
    }    
}

/*
 *  Routine:  termWriteReal
 *
 *  Description: writes several 'size' characters, a line of text pointed to by 'text'
 *				 to the terminal indicated by 'unit'.  A new line is not automatically appended 
 *				 so if one is needed it must be included in the text to be written.  It should not
 *				 return until the text has been written to the terminal.
 *
 *  Arguments: args -- the unit of the disk.
 *
 *  Return: none
 *  side effect: 
 */
int  termWriteReal(char *buffer, int bufferSize, int unitID, int *chars){
    
    if(unitID < 0 || unitID >= USLOSS_TERM_UNITS || bufferSize < 0){
	return -1;
    }

    // Send a line of output to the indicated terminal mbox
    MboxSend(Proc4Table[TermUnits[unitID] % MAXPROC].writerMbox, buffer, bufferSize);
   
    // Wait on private mbox for line to be written 
    MboxReceive(Proc4Table[getpid() % MAXPROC].writerMbox, NULL, 0);
	
    *chars = (int) strlen(buffer);

    return *chars;
}

