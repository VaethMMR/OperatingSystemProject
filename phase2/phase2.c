/* ------------------------------------------------------------------------
   phase2.c

   University of Arizona
   Computer Science 452

   ------------------------------------------------------------------------ */

#include <phase1.h>
#include <phase2.h>
#include <stdlib.h>
#include <string.h>
#include <usloss.h>

#include "message.h"

/* ------------------------- Prototypes ----------------------------------- */
int start1 (char *);
extern int start2 (char *);
int MboxCreate(int, int);
int MboxRelease(int);
int MboxSend(int, void*, int);
int MboxReceive(int, void*, int);
int MboxCondSend(int, void*, int);
int MboxCondReceive(int, void*, int);

void enqueueSendProc(mboxPtr);
void dequeueSendProc(mboxPtr);
void enqueueReceiveProc(mboxPtr);
void dequeueReceiveProc(mboxPtr);
void enqueueSlot(mboxPtr, slotPtr);
void dequeueSlot(mboxPtr);

void clearProc(int);
void clearSlot(int);
void clearBox(int);

void checkKernelMode(char*);
void disableInterrupts();
void enableInterrupts();

void printMboxes(void);
void printSlots(mboxPtr);
void printSendList(mboxPtr);
void printReceiveList(mboxPtr);

/* -------------------------- Globals ------------------------------------- */

int debugStart = 0;
int debugCreate = 0;
int debugSend = 0;
int debugReceive = 0;
int debugCondSend = 0;
int debugCondReceive = 0;
int debugRelease = 0;
int debugWait = 0;
int debugClean = 0;
int debugQueue = 0;
int debugInterrupts = 0;
int debugKernel = 0;
int dumpProcs = 0;
int printBoxes = 0;

// the mail boxes 
mailbox MailBoxTable[MAXMBOX];

// mail slots
mailSlot SlotTable[MAXSLOTS];

// process table
mboxProc ProcTable[MAXPROC];

// system call vector
void (*systemCallVec[MAXSYSCALLS])(systemArgs *);

// global mbox id
int boxID = 0;

// active mail slot counter
int activeSlots = 0;

/* -------------------------- Functions ----------------------------------- */

/* ------------------------------------------------------------------------
   Name - start1
   Purpose - Initializes mailboxes and interrupt vector.
             Start the phase2 test process.
   Parameters - one, default arg passed by fork1, not used here.
   Returns - one to indicate normal quit.
   Side Effects - lots since it initializes the phase2 data structures.
   ----------------------------------------------------------------------- */
int start1(char *arg)
{
    int kid_pid, status, i;
    if (DEBUG2 && debugStart)
        USLOSS_Console("start1(): at beginning\n");

    checkKernelMode("start1");

    // Disable interrupts
    disableInterrupts();

    // Initialize the mail box table, slots, & other data structures.
    
    // mailbox table
    for(i = 0; i < MAXMBOX; i++){
        clearBox(i);
    }
    
    // mail slot table
    for(i = 0; i < MAXSLOTS; i++){
        clearSlot(i);
    }
    
    // proc table
    for(i = 0; i < MAXPROC; i++){
        clearProc(i);
    }

    // Initialize USLOSS_IntVec and system call handlers,
    USLOSS_IntVec[USLOSS_CLOCK_INT] = clockHandler2;
    USLOSS_IntVec[USLOSS_DISK_INT] = diskHandler;
    USLOSS_IntVec[USLOSS_TERM_INT] = termHandler;
    USLOSS_IntVec[USLOSS_SYSCALL_INT] = syscallHandler;
    
    // initialize i/o mailboxes

    // 1 for clock
    MboxCreate(0, sizeof(int));
    
    // 2 for disk
    MboxCreate(0, sizeof(int));
    MboxCreate(0, sizeof(int));
    
    // 4 for terminal
    MboxCreate(0, sizeof(int));
    MboxCreate(0, sizeof(int));
    MboxCreate(0, sizeof(int));
    MboxCreate(0, sizeof(int));
    
    // initialize the systemCallVec to point to nullsys
    for(i = 0; i < MAXSYSCALLS; i++){
        systemCallVec[i] = nullsys;
    }
    
    enableInterrupts();

    // Create a process for start2, then block on a join until start2 quits
    if (DEBUG2 && debugStart)
        USLOSS_Console("start1(): fork'ing start2 process\n");
    kid_pid = fork1("start2", start2, NULL, 4 * USLOSS_MIN_STACK, 1);
    if ( join(&status) != kid_pid ) {
        USLOSS_Console("start2(): join returned something other than ");
        USLOSS_Console("start2's pid\n");
    }

    return 0;
} /* start1 */


/* ------------------------------------------------------------------------
   Name - MboxCreate
   Purpose - gets a free mailbox from the table of mailboxes and initializes it 
   Parameters - maximum number of slots in the mailbox and the max size of a msg
                sent to the mailbox.
   Returns - -1 to indicate that no mailbox was created, or a value >= 0 as the
             mailbox id.
   Side Effects - initializes one element of the mail box array. 
   ----------------------------------------------------------------------- */
int MboxCreate(int slots, int slot_size)
{
    int i, mboxID, mboxSlot;
    checkKernelMode("MboxCreate");
    disableInterrupts();
    
    // check for invalid slots or message sizes
    if(slots < 0 || slot_size > MAX_MESSAGE || slot_size < 0)
	return -1;
    
    //  check for an open mailbox
    for(i = 0; MailBoxTable[i % MAXMBOX].mboxID != EMPTYSLOT; i++){
        if(i > MAXMBOX){
            if(debugCreate && DEBUG2)
                USLOSS_Console("MboxCreate(): Mailbox full Returning -1\n");
            return -1;
        }
    }
    
    //=======================CREATE MAILBOX====================
    mboxID = i;
    mboxSlot = mboxID % MAXMBOX;
    MailBoxTable[mboxSlot].mboxID = mboxID;
    MailBoxTable[mboxSlot].numSlots = slots;
    MailBoxTable[mboxSlot].slotSize = slot_size;
    MailBoxTable[mboxSlot].activeSlots = 0;
    MailBoxTable[mboxSlot].status = ACTIVE;
    MailBoxTable[mboxSlot].pid = getpid();
	
    enableInterrupts();
    
    return mboxID;
} /* MboxCreate */

/* ------------------------------------------------------------------------
   Name - MboxRelease
   Purpose - Releases a previously created mailbox. Any process waiting on 
			 the mailbox should be zap’d. Note, however, that zap’ing does
			 not quite work. It would work for a high priority process releasing
			 low priority processes from the mailbox, but not the other way around.
			 You will need to devise a different means of handling processes that 
			 are blocked on a mailbox being released. Essentially, you will need 
			 to have a blocked process return -3 from the send or receive that caused
			 it to block. You will need to have the process that called MboxRelease
			 unblock all the blocked processes. When each of these processes awake
			 from the blockMe call inside send or receive, they will need to “notice”
			 that the mailbox has been released…
   Parameters - mailboxID of a mailbox to be released.
   Returns - zero if successful, -1 if mailboxID is not a mailbox in use, -3 if the
			 process was zap'd while releasing the mailbox.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxRelease(int mailboxID){
    slotPtr slot;
    mboxProcPtr cur;
    checkKernelMode("MboxRelease");
    disableInterrupts();
    mboxPtr mbox = &MailBoxTable[mailboxID % MAXMBOX];
    
    if(DEBUG2 && debugRelease){
        printSendList(mbox);
        printReceiveList(mbox);
    }
    
    if(mbox->status != ACTIVE){
        if(DEBUG2 && debugRelease){
            USLOSS_Console("MBoxRelease: mailbox %d is not active\n", mailboxID);
        }
        enableInterrupts();
        return -1;
    }
    
    // change mailbox to released and free slots
    mbox->status = RELEASED;
    for(slot = mbox->slotPtr; slot != NULL; slot = slot->nextSlotPtr){
        if(DEBUG2 && debugRelease){
            USLOSS_Console("MboxRelease: clearing slot %d\n", slot->slotID);
        }
        dequeueSlot(mbox);
        clearSlot(slot->slotID);
    }
    
    
    // unblock all blocked processes
    for(cur = mbox->sendPtr; cur != NULL; cur = mbox->sendPtr){
        int sendPid = cur->pid;
        if(DEBUG2 && debugRelease){
            USLOSS_Console("MboxRelease: unblocking %d\n", sendPid);
        }
        dequeueSendProc(mbox);
        unblockProc(sendPid);
        
        disableInterrupts();
    }
    
    for(cur = mbox->receivePtr; cur != NULL; cur = mbox->receivePtr){
        int receivePid = cur->pid;
        if(DEBUG2 && debugRelease){
            USLOSS_Console("MboxRelease: unblocking %d\n", receivePid);
        }
        dequeueReceiveProc(mbox);
        unblockProc(receivePid);
        
        disableInterrupts();
    }
    
    clearBox(mailboxID);
    
    if(isZapped()){
        if(DEBUG2 && debugRelease){
            USLOSS_Console("MboxRelease: Process %d was zapped\n", getpid());
        }
        enableInterrupts();
        return -3;
    }

    enableInterrupts();
    return 0;
} /* MboxRelease */

/* ------------------------------------------------------------------------
   Name - MboxSend
   Purpose - Put a message into a slot for the indicated mailbox.
             Block the sending process if no slot available.
   Parameters - mailbox id, pointer to data of msg, # of bytes in msg.
   Returns - zero if successful, -1 if invalid args.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxSend(int mbox_id, void *msg_ptr, int msg_size)
{
    mboxPtr mboxPtr;
    slotPtr slot;
    int i, failed = 0;
    checkKernelMode("MboxSend");
    disableInterrupts();
    
    if(activeSlots >= MAXSLOTS){
        if(DEBUG2 && debugSend){
            USLOSS_Console("MboxSend: mail slots full\n");
        }
        USLOSS_Halt(1);
    }
    
    if(mbox_id < 0 || mbox_id >= MAXMBOX){
        if(DEBUG2 && debugSend){
            USLOSS_Console("MboxSend: invalid mbox id %d\n", mbox_id);
        }
        failed = 1;
    }
    
    mboxPtr = &MailBoxTable[mbox_id % MAXMBOX];
    
    if(mboxPtr->status != ACTIVE){
        if(DEBUG2 && debugSend){
            USLOSS_Console("MboxSend: mailbox %d is not active\n", mbox_id);
        }
        failed = 1;
    }
    
    if(msg_size < 0 || msg_size > MAX_MESSAGE){
        if(DEBUG2 && debugSend){
            USLOSS_Console("MboxSend: invalid message size %d\n", msg_size);
        }
        failed = 1;
    }
    
    if(msg_size > mboxPtr->slotSize){
        if(DEBUG2 && debugSend){
            USLOSS_Console("MboxSend: invalid message size for mailbox %d: %d %d\n", mbox_id, msg_size, mboxPtr->slotSize);
        }
        failed = 1;
    }
    
    if(failed){
        enableInterrupts();
        return -1;
    }
    
    // CASE 1: Receivers blocked
    if(mboxPtr->receivePtr != NULL){
        // send message to receiver
        if(DEBUG2 && debugSend){
            USLOSS_Console("PUTS %d IN PID %d SIZE %d\n", msg_size, mboxPtr->receivePtr->pid, mboxPtr->receivePtr->msgSize);
        }
        
        int size = mboxPtr->receivePtr->msgSize;
        
        mboxPtr->receivePtr->msgSize = msg_size;
        memcpy(mboxPtr->receivePtr->message, msg_ptr, msg_size);
        
        //unblock receiver
        mboxPtr->status = ACTIVE;
        int receivePid = mboxPtr->receivePtr->pid;
        dequeueReceiveProc(mboxPtr);
        unblockProc(receivePid);
        
        disableInterrupts();
        
        if(msg_size > size){
            enableInterrupts();
            return -1;
        }
    }
    
    // CASE 2: slots available
    else if(mboxPtr->activeSlots < mboxPtr->numSlots){
        // send message to a slot
        
            if(DEBUG2 && debugSend){
                USLOSS_Console("MboxSend: Process %d is blocked\n", getpid());
            }
        
            // Assign mail slot for mailbox id
            for(i = 0; SlotTable[i].status != UNUSED; i++);
            slot = &SlotTable[i];
            slot->mboxID = mbox_id;
            slot->msgSize = msg_size;
            slot->status = ACTIVE;
            slot->nextSlotPtr = NULL;
    
            if(DEBUG2 && debugSend){
                USLOSS_Console("MboxSend: input size %d slot size %d\n", msg_size, slot->msgSize);
            }
    
            //memcpy message contents to slot
            memcpy(slot->message, msg_ptr, msg_size);
    
            if(DEBUG2 && debugSend){
                USLOSS_Console("MboxSend: input size %d slot size %d\n", msg_size, slot->msgSize);
            }
    
            //add slot to list of slots for mailbox
            enqueueSlot(mboxPtr, slot);
        
            if(DEBUG2 && debugSend){
                printSlots(mboxPtr);
            }
        
            // increment active slot counters
            mboxPtr->activeSlots++;
            activeSlots++;
        
            if(DEBUG2 && debugSend){
                USLOSS_Console("MboxSend: mbox %d %d active slots\n", mboxPtr->mboxID, mboxPtr-activeSlots);
                printMboxes();
            }
    
            if(DEBUG2 && debugSend && dumpProcs){
                dumpProcesses();
            }
    
            //unblock the the receiver if blocked
        if(mboxPtr->receivePtr != NULL){
            if(DEBUG2 && debugSend){
                USLOSS_Console("MboxSend: Process %d will unblock\n", mboxPtr->receivePtr->pid);
            }
    
            int receivePid = mboxPtr->receivePtr->pid;
            mboxPtr->receivePtr->status = ACTIVE;
                
            // remove proc from receiver blocks
            dequeueReceiveProc(mboxPtr);
                
            //unblock proc
            unblockProc(receivePid);
            
            disableInterrupts();
        }
        
            if(DEBUG2 && debugSend){
                //        printMboxes();
            }
        }
    
    // CASE 3: no available slots
    else {
        // block process until slot opens
        // if zapped or released return -3
            if(DEBUG2 && debugSend){
                USLOSS_Console("MboxSend: Process %d is blocked\n", getpid());
            }
        
        // store message data for receiver
        ProcTable[getpid() % MAXPROC].msgSize = msg_size;
        memcpy(ProcTable[getpid() % MAXPROC].message, msg_ptr, msg_size);
        
        // change status of sender to blocked
        ProcTable[getpid() % MAXPROC].status = SENDBLOCKED;
        
        //add blocked proc to mbox list of send blocks
        enqueueSendProc(mboxPtr);
        
        // Block process
        blockMe(SENDBLOCKED);
        
        disableInterrupts();

    }
    
    if(isZapped()){
        if(DEBUG2 && debugSend){
            USLOSS_Console("MboxSend: process %d is zapped\n", getpid());
        }
        enableInterrupts();
        return -3;
    }
    
    if(mboxPtr->status == RELEASED || mboxPtr->status == UNUSED){
        if(DEBUG2 && debugSend){
            USLOSS_Console("MboxSend: process %d is released\n", mbox_id);
        }
        mboxPtr->status = UNUSED;
        enableInterrupts();
        return -3;
    }
    
    enableInterrupts();
    return 0;
} /* MboxSend */


/* ------------------------------------------------------------------------
   Name - MboxReceive
   Purpose - Get a msg from a slot of the indicated mailbox.
             Block the receiving process if no msg available.
   Parameters - mailbox id, pointer to put data of msg, max # of bytes that
                can be received.
   Returns - actual size of msg if successful, -1 if invalid args.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxReceive(int mbox_id, void *msg_ptr, int msg_size)
{
    checkKernelMode("MboxReceive");
    disableInterrupts();
    
    mboxPtr mboxPtr;
    slotPtr cur;
    slotPtr slot;
    int i, size, failed = 0;
    
    if(mbox_id < 0 || mbox_id >= MAXMBOX){
        if(DEBUG2 && debugReceive){
            USLOSS_Console("MboxReceive: invalid mbox id %d\n", mbox_id);
        }
        failed = 1;
    }
    
    mboxPtr = &MailBoxTable[mbox_id % MAXMBOX];
    
    if(mboxPtr->status != ACTIVE){
        if(DEBUG2 && debugReceive){
            USLOSS_Console("MboxReceive: mailbox %d is not active\n", mbox_id);
        }
        failed = 1;
    }
    
    if(msg_size < 0 || msg_size > MAX_MESSAGE){
        if(DEBUG2 && debugReceive){
            USLOSS_Console("MboxReceive: invalid message size %d\n", msg_size);
        }
        failed = 1;
    }
    
    if(failed){
        enableInterrupts();
        return -1;
    }
    
    if(DEBUG2 && debugReceive){
        USLOSS_Console("mbox %d %d active slots\n", mboxPtr->mboxID, mboxPtr->activeSlots);
        printMboxes();
        printSlots(mboxPtr);
    }
    
    // CASE 1: no active slots
    if(mboxPtr->activeSlots == 0){
        // block process until slot opens
        if(DEBUG2 && debugReceive){
            USLOSS_Console("MboxReceive: process %d is blocked\n", getpid());
        }
        
        //zero slot mailbox: unblock sender if ther is a sender
        if(mboxPtr->numSlots == 0){
            if(mboxPtr->sendPtr != NULL){
                if(DEBUG2 && debugReceive){
                    USLOSS_Console("Process %d will unblock\n", mboxPtr->sendPtr->pid);
                }
                
                // copy message of blocked sender to receiver
                memcpy(msg_ptr, mboxPtr->sendPtr->message, mboxPtr->sendPtr->msgSize);
                size = mboxPtr->sendPtr->msgSize;
                
                if(DEBUG2 && debugReceive){
                    USLOSS_Console("sender unblocked message copied\n");
                    printSlots(mboxPtr);
                }
                
                // get pid of process to unblock
                int sendPid = mboxPtr->sendPtr->pid;
                
                //change status to active
                mboxPtr->sendPtr->status = ACTIVE;
                                
                // remove proc from receiver blocks
                dequeueSendProc(mboxPtr);
                
                //unblock proc
                unblockProc(sendPid);
                
                disableInterrupts();
            }
            else {
                
                // change status of receiver to blocked
                ProcTable[getpid() % MAXPROC].status = RECEIVEBLOCKED;
                ProcTable[getpid() % MAXPROC].msgSize = msg_size;
                
                //enqueue proc to list of receive blocks
                enqueueReceiveProc(mboxPtr);
                
                // Block process
                blockMe(RECEIVEBLOCKED);
                
                disableInterrupts();
                
                if(DEBUG2 && debugReceive){
                    USLOSS_Console("GETS %d %s FROM PID %d SIZE %d\n", ProcTable[getpid() % MAXPROC].msgSize,
                                   ProcTable[getpid() % MAXPROC].message, getpid(), msg_size);
                }
                
                    if(ProcTable[getpid() % MAXPROC].msgSize > msg_size){
                        if(DEBUG2 && debugReceive){
                            USLOSS_Console("MboxReceive: invalid message size for mailbox %d: %d %d\n", mbox_id, msg_size, ProcTable[getpid() % MAXPROC].msgSize);
                        }
                        enableInterrupts();
                        return -1;
                    }
                
                size = ProcTable[getpid() % MAXPROC].msgSize;
                memcpy(msg_ptr, ProcTable[getpid() % MAXPROC].message, ProcTable[getpid() % MAXPROC].msgSize);
            }
        }
        else {
        
            // change status of receiver to blocked
            ProcTable[getpid() % MAXPROC].status = RECEIVEBLOCKED;
            ProcTable[getpid() % MAXPROC].msgSize = msg_size;
    
            //enqueue proc to list of receive blocks
            enqueueReceiveProc(mboxPtr);

            // Block process
            blockMe(RECEIVEBLOCKED);
            
            disableInterrupts();

            if(DEBUG2 && debugReceive){
                USLOSS_Console("GETS %d FROM PID %d SIZE %d\n", ProcTable[getpid() % MAXPROC].msgSize, getpid(), msg_size);
            }
            
            if(ProcTable[getpid() % MAXPROC].msgSize > msg_size){
                if(DEBUG2 && debugReceive){
                    USLOSS_Console("MboxReceive: invalid message size for mailbox %d: %d %d\n", mbox_id, msg_size, ProcTable[getpid() % MAXPROC].msgSize);
                }
                enableInterrupts();
                return -1;
            }
        
            size = ProcTable[getpid() % MAXPROC].msgSize;
            memcpy(msg_ptr, ProcTable[getpid() % MAXPROC].message, ProcTable[getpid() % MAXPROC].msgSize);
        }
    }
    
    else{
        // CASE 2: available active slot
        cur = mboxPtr->slotPtr;
            
        if(cur->msgSize > msg_size){
            if(DEBUG2 && debugReceive){
                USLOSS_Console("MboxReceive: message is too large for receiver buffer %d %d\n", cur->msgSize,   msg_size);
            }
            enableInterrupts();
            return -1;
        }
            
        memcpy(msg_ptr, cur->message, cur->msgSize);
            
        size = cur->msgSize;
        
        // remove the slot
        dequeueSlot(mboxPtr);
    
        // decrement active slot counters
        mboxPtr->activeSlots--;
        activeSlots--;
    
        // CASE 3: blocked senders
        //        //unblock the the sender if blocked
        if(mboxPtr->sendPtr != NULL){
            if(DEBUG2 && debugReceive){
                USLOSS_Console("Process %d will unblock\n", mboxPtr->sendPtr->pid);
            }
    
            //assign new slot and put message in
            for(i = 0; SlotTable[i].status != UNUSED; i++);
            slot = &SlotTable[i];
            slot->mboxID = mbox_id;
            slot->msgSize = mboxPtr->sendPtr->msgSize;
            slot->status = ACTIVE;
            slot->nextSlotPtr = NULL;
    
            // copy message of blocked sender to mail slot
            memcpy(slot->message, mboxPtr->sendPtr->message, slot->msgSize);

            // enqueue slot
            enqueueSlot(mboxPtr, slot);
    
            if(DEBUG2 && debugReceive){
                USLOSS_Console("sender unblocked message copied\n");
                printSlots(mboxPtr);
            }
    
            // get pid of process to unblock
            int sendPid = mboxPtr->sendPtr->pid;
    
            //change status to active
            mboxPtr->sendPtr->status = ACTIVE;
            
            mboxPtr->activeSlots++;
            activeSlots++;
    
            // remove proc from receiver blocks
            dequeueSendProc(mboxPtr);
            
            //unblock proc
            unblockProc(sendPid);
            
            disableInterrupts();
        }
    }
    
    if(isZapped()){
        if(DEBUG2 && debugReceive){
            USLOSS_Console("MboxReceive: process %d is zapped\n", getpid());
        }
        enableInterrupts();
        return -3;
    }
        
    if(mboxPtr->status == RELEASED || mboxPtr->status == UNUSED){
        if(DEBUG2 && debugReceive){
            USLOSS_Console("MboxReceive: mailbox %d is released\n", mbox_id);
        }
        mboxPtr->status = UNUSED;
        enableInterrupts();
        return -3;
    }
    
    enableInterrupts();
    return size;
} /* MboxReceive */

/* ------------------------------------------------------------------------
   Name - MboxCondSend
   Purpose - Conditionally send a message to a mailbox. Do not block the 
			 invoking process. Rather, if there is no empty slot in the 
			 mailbox in which to place the message, the value -2 is returned.
			 Also return -2 in the case that all the mailbox slots in the
			 system are used and none are available to allocate for this message.
   Parameters - mailbox id, pointer to put data of msg, max # of bytes that
                can be received.
   Returns - actual size of msg if successful, -1 if invalid args.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxCondSend(int mbox_id, void *msg_ptr, int msg_size)
{
    mboxPtr mboxPtr;
    slotPtr slot;
    int i, failed = 0;
    checkKernelMode("MboxCondSend");
    disableInterrupts();
    
    if(DEBUG2 && debugCondSend){
        USLOSS_Console("MboxCondSend: started\n");
    }
    
    if(activeSlots >= MAXSLOTS){
        if(DEBUG2 && debugCondSend){
            USLOSS_Console("MboxCondSend: mail slots full\n");
        }
        enableInterrupts();
        return -2;
    }
    
    if(mbox_id < 0 || mbox_id >= MAXMBOX){
        if(DEBUG2 && debugCondSend){
            USLOSS_Console("MboxCondSend: invalid mbox id %d\n", mbox_id);
        }
        failed = 1;
    }
    
    mboxPtr = &MailBoxTable[mbox_id % MAXMBOX];
    
    if(mboxPtr->status != ACTIVE){
        if(DEBUG2 && debugCondSend){
            USLOSS_Console("MboxCondSend: mailbox %d is not active\n", mbox_id);
        }
        failed = 1;
    }
    
    if(msg_size < 0 || msg_size > MAX_MESSAGE){
        if(DEBUG2 && debugCondSend){
            USLOSS_Console("MboxCondSend: invalid message size %d\n", msg_size);
        }
        failed = 1;
    }
    
    if(msg_size > mboxPtr->slotSize){
        if(DEBUG2 && debugCondSend){
            USLOSS_Console("MboxCondSend: invalid message size for mailbox %d: %d %d\n", mbox_id, msg_size, mboxPtr->slotSize);
        }
        failed = 1;
    }
    if(failed){
        enableInterrupts();
        return -1;
    }
    
    // CASE 1: Receivers blocked
    if(mboxPtr->receivePtr != NULL){
        // send message to receiver
        if(DEBUG2 && debugCondSend){
            USLOSS_Console("PUTS %d %s IN PID %d SIZE %d\n", msg_size, msg_ptr, mboxPtr->receivePtr->pid, mboxPtr->receivePtr->msgSize);
        }
        
        int size = mboxPtr->receivePtr->msgSize;
        
        mboxPtr->receivePtr->msgSize = msg_size;
        memcpy(&mboxPtr->receivePtr->message, msg_ptr, msg_size);
        
        //unblock receiver
        mboxPtr->status = ACTIVE;
        int receivePid = mboxPtr->receivePtr->pid;
        dequeueReceiveProc(mboxPtr);
        unblockProc(receivePid);
        
        disableInterrupts();
        
        if(msg_size > size){
            enableInterrupts();
            return -1;
        }
    }
    
    // CASE 2: slots available
    else if(mboxPtr->activeSlots < mboxPtr->numSlots){
        // send message to a slot
        
        if(DEBUG2 && debugCondSend){
            USLOSS_Console("MboxCondSend: Process %d is blocked\n", getpid());
        }
        
        // Assign mail slot for mailbox id
        for(i = 0; SlotTable[i].status != UNUSED; i++);
        slot = &SlotTable[i];
        slot->mboxID = mbox_id;
        slot->msgSize = msg_size;
        slot->status = ACTIVE;
        slot->nextSlotPtr = NULL;
        
        if(DEBUG2 && debugCondSend){
            USLOSS_Console("MboxCondSend: input size %d slot size %d\n", msg_size, slot->msgSize);
        }
        
        //memcpy message contents to slot
        memcpy(slot->message, msg_ptr, msg_size);
        
        if(DEBUG2 && debugCondSend){
            USLOSS_Console("MboxCondSend: input size %d slot size %d\n", msg_size, slot->msgSize);
        }
        
        //add slot to list of slots for mailbox
        enqueueSlot(mboxPtr, slot);
        
        if(DEBUG2 && debugCondSend){
            printSlots(mboxPtr);
        }
        
        // increment active slot counters
        mboxPtr->activeSlots++;
        activeSlots++;
        
        if(DEBUG2 && debugCondSend){
            USLOSS_Console("MboxCondSend: mbox %d %d active slots\n", mboxPtr->mboxID, mboxPtr-activeSlots);
            printMboxes();
        }
        
        if(DEBUG2 && debugCondSend && dumpProcs){
            dumpProcesses();
        }
        
        //unblock the the receiver if blocked
        if(mboxPtr->receivePtr != NULL){
            if(DEBUG2 && debugCondSend){
                USLOSS_Console("MboxCondSend: Process %d will unblock\n", mboxPtr->receivePtr->pid);
            }
            
            int receivePid = mboxPtr->receivePtr->pid;
            mboxPtr->receivePtr->status = ACTIVE;
            
            // remove proc from receiver blocks
            dequeueReceiveProc(mboxPtr);
            
            //unblock proc
            unblockProc(receivePid);
            disableInterrupts();
        }
        
        if(DEBUG2 && debugCondSend){
            //        printMboxes();
        }
    }
    
    // CASE 3: no available slots
    else {
        enableInterrupts();
        return -2;
    }
    
    if(isZapped()){
        if(DEBUG2 && debugCondSend){
            USLOSS_Console("MboxCondSend: process %d is zapped\n", getpid());
        }
        enableInterrupts();
        return -3;
    }
    
    if(mboxPtr->status == RELEASED || mboxPtr->status == UNUSED){
        if(DEBUG2 && debugReceive){
            USLOSS_Console("MboxReceive: mailbox %d is released\n", mbox_id);
        }
        mboxPtr->status = UNUSED;
        enableInterrupts();
        return -3;
    }
    
    enableInterrupts();
    return 0;
} /* MboxCondSend */

/* ------------------------------------------------------------------------
   Name - MboxCondRecieve
   Purpose - Conditionally receive a message from a mailbox. Do not block the
			 invoking process. Rather, if there is no message in the mailbox,
			 the value -2 is returned. 
   Parameters - mailbox id, pointer to put data of msg, max # of bytes that
                can be received.
   Returns - actual size of msg if successful, -1 if invalid args.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxCondReceive(int mbox_id, void *msg_ptr, int msg_size)
{
    checkKernelMode("MboxCondReceive");
    mboxPtr mboxPtr;
    slotPtr cur;
    slotPtr slot;
    int i, size, failed = 0;
    
    disableInterrupts();
    if(mbox_id < 0 || mbox_id >= MAXMBOX){
        if(DEBUG2 && debugCondReceive){
            USLOSS_Console("MboxCondReceive: invalid mbox id %d\n", mbox_id);
        }
        failed = 1;
    }
    
    mboxPtr = &MailBoxTable[mbox_id % MAXMBOX];
    
    if(mboxPtr->status != ACTIVE){
        if(DEBUG2 && debugCondReceive){
            USLOSS_Console("MboxCondReceive: mailbox %d is not active\n", mbox_id);
        }
        failed = 1;
    }
    
    if(msg_size < 0 || msg_size > MAX_MESSAGE){
        if(DEBUG2 && debugCondReceive){
            USLOSS_Console("MboxCondReceive: invalid message size %d\n", msg_size);
        }
        failed = 1;
    }
    
    if(failed){
        enableInterrupts();
        return -1;
    }
    
    if(DEBUG2 && debugCondReceive){
        USLOSS_Console("mbox %d %d active slots\n", mboxPtr->mboxID, mboxPtr->activeSlots);
        printMboxes();
        printSlots(mboxPtr);
    }
    
    // CASE 1: no active slots
    if(mboxPtr->activeSlots == 0){
        // block process until slot opens
        if(DEBUG2 && debugCondReceive){
            USLOSS_Console("MboxCondReceive: process %d is blocked\n", getpid());
        }
        
        //zero slot mailbox: unblock sender if ther is a sender
        if(mboxPtr->numSlots == 0){
            if(mboxPtr->sendPtr != NULL){
                if(DEBUG2 && debugCondReceive){
                    USLOSS_Console("Process %d will unblock\n", mboxPtr->sendPtr->pid);
                }
                
                // copy message of blocked sender to receiver
                memcpy(msg_ptr, mboxPtr->sendPtr->message, mboxPtr->sendPtr->msgSize);
                size = mboxPtr->sendPtr->msgSize;
                
                if(DEBUG2 && debugCondReceive){
                    USLOSS_Console("sender unblocked message copied\n");
                    printSlots(mboxPtr);
                }
                
                // get pid of process to unblock
                int sendPid = mboxPtr->sendPtr->pid;
                
                //change status to active
                mboxPtr->sendPtr->status = ACTIVE;
                
                // remove proc from receiver blocks
                dequeueSendProc(mboxPtr);
                
                //unblock proc
                unblockProc(sendPid);
                
                disableInterrupts();
            }
            else {
                enableInterrupts();
                return -2;
            }
        }
        else {
            enableInterrupts();
            return -2;
        }
    }
    
    else{
        // CASE 2: active slot
        cur = mboxPtr->slotPtr;
        
        if(cur->msgSize > msg_size){
            if(DEBUG2 && debugCondReceive){
                USLOSS_Console("MboxCondReceive: message is too large for receiver buffer %d %d\n", cur->msgSize,   msg_size);
            }
            enableInterrupts();
            return -1;
        }
        
        memcpy(msg_ptr, cur->message, cur->msgSize);
        
        size = cur->msgSize;
        
        // remove the slot
        dequeueSlot(mboxPtr);
        
        // decrement active slot counters
        mboxPtr->activeSlots--;
        activeSlots--;
        
        // CASE 3: blocked senders
        //        //unblock the the sender if blocked
        if(mboxPtr->sendPtr != NULL){
            if(DEBUG2 && debugCondReceive){
                USLOSS_Console("Process %d will unblock\n", mboxPtr->sendPtr->pid);
            }
            
            //assign new slot and put message in
            for(i = 0; SlotTable[i].status != UNUSED; i++);
            slot = &SlotTable[i];
            slot->mboxID = mbox_id;
            slot->msgSize = mboxPtr->sendPtr->msgSize;
            slot->status = ACTIVE;
            slot->nextSlotPtr = NULL;
            
            // copy message of blocked sender to mail slot
            memcpy(slot->message, mboxPtr->sendPtr->message, slot->msgSize);
            
            // enqueue slot
            enqueueSlot(mboxPtr, slot);
            
            if(DEBUG2 && debugCondReceive){
                USLOSS_Console("sender unblocked message copied\n");
                printSlots(mboxPtr);
            }
            
            // get pid of process to unblock
            int sendPid = mboxPtr->sendPtr->pid;
            
            //change status to active
            mboxPtr->sendPtr->status = ACTIVE;
            
            mboxPtr->activeSlots++;
            activeSlots++;
            
            // remove proc from receiver blocks
            dequeueSendProc(mboxPtr);
            
            //unblock proc
            unblockProc(sendPid);
            
            disableInterrupts();
        }
    }
    
    if(isZapped()){
        if(DEBUG2 && debugCondReceive){
            USLOSS_Console("MboxCondReceive: process %d is zapped\n", getpid());
        }
        enableInterrupts();
        return -3;
    }
    
    if(mboxPtr->status == RELEASED || mboxPtr->status == UNUSED){
        if(DEBUG2 && debugReceive){
            USLOSS_Console("MboxReceive: mailbox %d is released\n", mbox_id);
        }
        mboxPtr->status = UNUSED;
        enableInterrupts();
        return -3;
    }
    
    enableInterrupts();
    return size;
} /* MboxCondReceive */

/* ------------------------------------------------------------------------
   Name - waitDevice
   Purpose - Do a receive operation on the mailbox associated with the given 
			 unit of the device type.The device types are defined in usloss.h.
			 The appropriate device mailbox is sent a message every time an interrupt
			 is generated by the I/O device, with the exception of the clock device 
			 which should only be sent a message every 100,000 time units (every 5 interrupts).
			 This routine will be used to synchronize with a device driver process in the
			 next phase. waitDevice returns the device’s status register in *status.
   Parameters - device type, unit type, status register.
   Returns - if recieve operation on the associated maiblox is run, -1 if process was
			 zap'd while waiting.
   Side Effects - appropriate device mailbox is sent a message every time an interrupt is
				  generated by the I/O device.
   ----------------------------------------------------------------------- */
int waitDevice(int type, int unit, int *status){
    checkKernelMode("waitDevice");
    int mboxID;
    char *boxes[7] = {"CLOCKBOX", "DISKBOX1", "DISKBOX2", "TERMBOX1", "TERMBOX2", "TERMBOX3", "TERMBOX4"  };
    
    if(DEBUG2 && debugWait){
        USLOSS_Console("waitDevice: started\n");
        USLOSS_Console("waitDevice: params: %d %d\n", type, unit);
    }
    switch (type) {
        case USLOSS_CLOCK_DEV:
            if(DEBUG2 && debugWait){
                USLOSS_Console("waitDevice: Clock called\n");
            }
            mboxID = CLOCKBOX;
            break;
        case USLOSS_TERM_DEV:
            if(DEBUG2 && debugWait){
                USLOSS_Console("waitDevice: Terminal called\n");
            }
            switch (unit) {
                case 0:
                    mboxID = TERMBOX1;
                    break;
                case 1:
                    mboxID = TERMBOX2;
                    break;
                case 2:
                    mboxID = TERMBOX3;
                    break;
                case 3:
                    mboxID = TERMBOX4;
                    break;
                    
                default:
                    mboxID = -1;
            }
            break;
        case USLOSS_DISK_DEV:
            if(DEBUG2 && debugWait){
                USLOSS_Console("waitDevice: Disk called\n");
            }
            switch (unit) {
                case 0:
                    mboxID = DISKBOX1;
                    break;
                case 1:
                    mboxID = DISKBOX2;
                    break;
                    
                default:
                    mboxID = -1;
            }
            break;
        default:
            mboxID = -1;
            break;
    }

    if(DEBUG2 && debugWait && mboxID != -1){
        USLOSS_Console("waitDevice: received mbox %s\n", boxes[mboxID]);
    }
    MboxReceive(mboxID, status, sizeof(int));
    
    if(isZapped()){
        if(DEBUG2 && debugWait){
            USLOSS_Console("Proc %d was zapped\n", getpid());
        }
        return -1;
    }
	return 0;
}

/* ------------------------------------------------------------------------
 Name - check_io
 Purpose - Checks the I/O mailboxes
 Parameters - none
 Returns - 1 if at least one process has been blocked on an i/o mailbox, 0 if not
 Side Effects - none
 ------------------------------------------------------------------------ */
int check_io(){
    // the first 6 mailboxes are i/o mailboxes
    int i;
    for(i = 0; i < 7; i++){
        if(MailBoxTable[i].sendPtr != NULL || MailBoxTable[i].receivePtr != NULL)
            return 1;
    }
    return 0;
}/* check_io */

/* ------------------------------------------------------------------------
 Name - enqueueSendProc
 Purpose - Enqueues the current process to that mailboxes sender list
 Parameters - The mailbox whose sender list to enqueue
 Returns - none
 Side Effects - none
 ------------------------------------------------------------------------ */
void enqueueSendProc(mboxPtr mboxPtr){
    ProcTable[getpid() % MAXPROC].pid = getpid();
    mboxProcPtr ptr = &ProcTable[getpid() % MAXPROC];
    mboxProcPtr cur = mboxPtr->sendPtr;
    if(cur == NULL){
        mboxPtr->sendPtr = ptr;
    }
    else{
        for(; cur->nextSendPtr != NULL; cur = cur->nextSendPtr);
        cur->nextSendPtr = ptr;
    }
    if(DEBUG2 && debugQueue){
        printSendList(mboxPtr);
    }
}/* enqueueToReadyList */

/* ------------------------------------------------------------------------
 Name - dequeueSendProc
 Purpose - Dequeue a process from the mailbox's sender list
 Parameters - The mailbox whose sender list to dequeue
 Returns - none
 Side Effects -  none
 ------------------------------------------------------------------------ */
void dequeueSendProc(mboxPtr mboxPtr){
    mboxProcPtr cur = mboxPtr->sendPtr;
    if(cur != NULL){
        mboxPtr->sendPtr = cur->nextSendPtr;
        cur->nextSendPtr = NULL;
    }
    if(DEBUG2 && debugQueue){
        printSendList(mboxPtr);
    }
}/* dequeueFromReadyList */

/* ------------------------------------------------------------------------
 Name - enqueueReceiveProc
 Purpose - Enqueue a process from the mailbox's receiver list
 Parameters - The mailbox whose receiver list to enqueue
 Returns - none
 Side Effects -  none
 ------------------------------------------------------------------------ */
void enqueueReceiveProc(mboxPtr mboxPtr){
    ProcTable[getpid() % MAXPROC].pid = getpid();
    mboxProcPtr ptr = &ProcTable[getpid() % MAXPROC];
    mboxProcPtr cur = mboxPtr->receivePtr;
    if(cur == NULL){
        mboxPtr->receivePtr = ptr;
    }
    else{
        for(; cur->nextReceivePtr != NULL; cur = cur->nextReceivePtr);
        cur->nextReceivePtr = ptr;
    }
    if(DEBUG2 && debugQueue){
        printReceiveList(mboxPtr);
    }
}/* enqueueToReadyList */

/* ------------------------------------------------------------------------
 Name - dequeueReceiveProc
 Purpose - Dequeue a process from the mailbox's receiver list
 Parameters - The mailbox whose receiver list to dequeue
 Returns - none
 Side Effects -  none
 ------------------------------------------------------------------------ */
void dequeueReceiveProc(mboxPtr mboxPtr){
    mboxProcPtr cur = mboxPtr->receivePtr;
    if(cur != NULL){
        mboxPtr->receivePtr = cur->nextReceivePtr;
        cur->nextReceivePtr = NULL;
    }
    if(DEBUG2 && debugQueue){
        printReceiveList(mboxPtr);
    }
}/* dequeueFromReadyList */

/* ------------------------------------------------------------------------
 Name - enqueueSlot
 Purpose - Enqueues a mailSlot to the mailbox's list of assigned mail slots.
 Parameters - The mailbox whose slot list to enqueue
 Returns - none
 Side Effects - none
 ------------------------------------------------------------------------ */
void enqueueSlot(mboxPtr mboxPtr, slotPtr slot){
    slotPtr cur = mboxPtr->slotPtr;
    if(cur == NULL){
         mboxPtr->slotPtr = slot;
    }
    else{
        for(; cur->nextSlotPtr != NULL; cur = cur->nextSlotPtr);
        cur->nextSlotPtr = slot;
    }
}/* enqueueToReadyList */

/* ------------------------------------------------------------------------
 Name - dequeueSlot
 Purpose - Dequeues a mailSlot to the mailbox's list of assigned mail slots.
 Parameters - The mailbox whose slot list to dequeue 
 Returns - none
 Side Effects - none
 ------------------------------------------------------------------------ */
void dequeueSlot(mboxPtr mboxPtr){
    slotPtr cur = mboxPtr->slotPtr;
    if(cur != NULL){
        mboxPtr->slotPtr = cur->nextSlotPtr;
        cur->nextSlotPtr = NULL;
        clearSlot(cur->slotID);
    }
}/* dequeueFromReadyList */

void checkKernelMode(char *name){
    int byte = USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet();
    if(DEBUG2 && debugKernel){
        USLOSS_Console("checkKernelMode: from %s byte: %d\n", name, byte);
    }
    if(byte != 1){
        if(DEBUG2 && debugKernel){
            USLOSS_Console("%s: not in kernel mode\n", name);
        }
        USLOSS_Halt(1);
    }
}

/* ------------------------------------------------------------------------
 Name - enableInterrupts
 Purpose - The operation enables interrupts.
 Parameters - none
 Returns - none
 Side Effects - Turns on interrupts if we are in kernel mode.
		will halt if not in kernel mode
 ------------------------------------------------------------------------ */
void enableInterrupts()
{
    int byte;
    // turn the interrupts ON iff we are in kernel mode
    // check to determine if we are in kernel mode
    checkKernelMode("enableInterrupts");
    
    byte = USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT;
    // We ARE in kernel mode
    USLOSS_PsrSet(byte);
    if(DEBUG2 && debugInterrupts){
        USLOSS_Console("enable interrupts: byte: %d\n", byte);
    }
} /* enableInterrupts */


/* ------------------------------------------------------------------------
 Name - disableInterrupts
 Purpose - The operation disables the interrupts.
 Parameters - none
 Returns - none
 Side Effects - Turns off interrupts if we are in kernel mode.
		will halt if not in kernel mode
 ------------------------------------------------------------------------ */
void disableInterrupts()
{
    int byte;
    // turn the interrupts OFF iff we are in kernel mode
    // check to determine if we are in kernel mode
    checkKernelMode("disableInterrupts");
    
    byte = USLOSS_PsrGet() & ~USLOSS_PSR_CURRENT_INT;
    // We ARE in kernel mode
    USLOSS_PsrSet( byte );
    
    if(DEBUG2 && debugInterrupts){
        USLOSS_Console("disable interrupts: byte: %d\n", byte);
    }
} /* disableInterrupts */

/* ------------------------------------------------------------------------
 Name - clearSlot
 Purpose - clear contents stored in slot
 Parameters - index into the slot table 
 Returns - none
 Side Effects - none
 ------------------------------------------------------------------------ */
void clearSlot(int idx){
    int i;
    SlotTable[idx].mboxID = EMPTYSLOT;
    SlotTable[idx].status = UNUSED;
    for(i = 0; i < MAX_MESSAGE; i++){
        SlotTable[idx].message[i] = '\0';
    }
    SlotTable[idx].msgSize = -1;
    SlotTable[idx].slotID = idx;
}

/* ------------------------------------------------------------------------
 Name - clearBox
 Purpose - clear contents stored in mailbox
 Parameters - index into the mailbox table             
 Returns - none
 Side Effects - none
 ------------------------------------------------------------------------ */
void clearBox(int idx){
    MailBoxTable[idx].mboxID = EMPTYSLOT;
    MailBoxTable[idx].numSlots = -1;
    MailBoxTable[idx].slotSize = -1;
    MailBoxTable[idx].activeSlots = -1;
    MailBoxTable[idx].slotPtr = NULL;
    MailBoxTable[idx].pid = -1;
}

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
}

/* ------------------------------------------------------------------------
 Name - printMboxes
 Purpose - For debugging: print the contents of all mailboxes in the mailbox table
 Parameters - none
 Returns - none
 Side Effects - none
 ------------------------------------------------------------------------ */
void printMboxes(){
    int i;
    if(DEBUG2 && printBoxes){
        USLOSS_Console("i\tID\tPID\tSLOTS\tACTIVE\tSTATUS\n");
        for(i = 0; i < MAXMBOX; i++){
            USLOSS_Console("%d\t%d\t%d\t%d\t%d\t%d\n", i, MailBoxTable[i].mboxID, MailBoxTable[i].pid,  MailBoxTable[i].numSlots, MailBoxTable[i].activeSlots, MailBoxTable[i].status);
        }
    }
}

/* ------------------------------------------------------------------------
 Name - printSlots
 Purpose - For debugging: print the contents of all mail slots in the given mailbox
 Parameters - the mailbox whose slots to print
 Returns - none
 Side Effects - none
 ------------------------------------------------------------------------ */
void printSlots(mboxPtr mbox){
    slotPtr cur;
    USLOSS_Console("MBOX\tID\tSTATUS\tSIZE\tMESSAGE\n");
    for(cur = mbox->slotPtr; cur != NULL; cur = cur->nextSlotPtr){
        USLOSS_Console("%d\t%d\t%d\t%d\t%s\n", cur->mboxID, cur->slotID, cur->status, cur->msgSize, cur->message);
    }
}

/* ------------------------------------------------------------------------
 Name - printSendList
 Purpose - For debugging: print the list of senders on a mailboxes sender list
 Parameters - the mailbox whose senders to print
 Returns - none
 Side Effects - none
 ------------------------------------------------------------------------ */
void printSendList(mboxPtr mbox){
    mboxProcPtr cur;
    USLOSS_Console("printSendList: |MBOX %d|", mbox->mboxID);
    for(cur = mbox->sendPtr; cur != NULL; cur = cur->nextSendPtr){
        USLOSS_Console("->|PID %d|", cur->pid);
    }
    USLOSS_Console("\n");
}

/* ------------------------------------------------------------------------
 Name - printReceiveList
 Purpose - For debugging: print the list of receivers on a mailboxes sender list
 Parameters - the mailbox whose receivers to print
 Returns - none
 Side Effects - none
 ------------------------------------------------------------------------ */
void printReceiveList(mboxPtr mbox){
    mboxProcPtr cur;
    USLOSS_Console("printReceiveList: |MBOX %d|", mbox->mboxID);
    for(cur = mbox->receivePtr; cur != NULL; cur = cur->nextReceivePtr){
        USLOSS_Console("->|PID %d|", cur->pid);
    }
    USLOSS_Console("\n");
}
