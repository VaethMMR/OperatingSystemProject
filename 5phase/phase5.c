/*
 * phase5.c
 *
 * Implementation of a virtual memory (VM) system that supports demand
 * paging.  USLOSS MMU is used to configure a region of virtual memory
 * whose contents are process-specific.
 *
 * MMU is to implement a single-level page table so each process will
 * have its own page table for the VM region and will therefore have
 * its own view of what the VM region contains. Tags and protection bits
 * will not be needed.
 */


#include <assert.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <phase4.h>
#include <phase5.h>
#include <usyscall.h>
#include <libuser.h>
#include <vm.h>			// ***** NEW HEADER FILE, 12/1/16 *****/
#include <string.h>

extern void mbox_create(sysargs *args_ptr);
extern void mbox_release(sysargs *args_ptr);
extern void mbox_send(sysargs *args_ptr);
extern void mbox_receive(sysargs *args_ptr);
extern void mbox_condsend(sysargs *args_ptr);
extern void mbox_condreceive(sysargs *args_ptr);
void * vmInitReal(int mappings, int pages, int frames, int pagers)

static Process processes[MAXPROC];
int vmInitialized = 0;

FaultMsg faults[MAXPROC]; /* Note that a process can have only
                           * one fault at a time, so we can
                           * allocate the messages statically
                           * and index them by pid. */
VmStats  vmStats;
int vmRegion;

// clock hand
int clockHand;

// frame table and size
FTE *frameTable;
int frameTableSize;

// signal pager death
int pagerkill = 0;

// integer array for disk content
int *diskBlocks;
int numBlocks;
int DBPerTrack;
int sectsPerDB;

static void
FaultHandler(int  type,  // USLOSS_MMU_INT
             void *arg); // Offset within VM region

static void vmInit(sysargs *sysargsPtr);
static void vmDestroy(sysargs *sysargsPtr);

int faultMailBox;
int clockHandMbox;
int dBMbox;

// keeps track of pager PIDs to kill them later
int pagerPIDS[MAXPAGERS];

/*
 *----------------------------------------------------------------------
 *
 * start4 --
 *
 * Initializes the VM system call handlers. 
 *
 * Results:
 *      MMU return status
 *
 * Side effects:
 *      The MMU is initialized.
 *
 *----------------------------------------------------------------------
 */
int
start4(char *arg)
{
    int pid;
    int result;
    int status;

    /* to get user-process access to mailbox functions */
    systemCallVec[SYS_MBOXCREATE]      = mboxCreate;
    systemCallVec[SYS_MBOXRELEASE]     = mboxRelease;
    systemCallVec[SYS_MBOXSEND]        = mboxSend;
    systemCallVec[SYS_MBOXRECEIVE]     = mboxReceive;
    systemCallVec[SYS_MBOXCONDSEND]    = mboxCondsend;
    systemCallVec[SYS_MBOXCONDRECEIVE] = mboxCondreceive;

    /* user-process access to VM functions */
    systemCallVec[SYS_VMINIT]    = vmInit;
    systemCallVec[SYS_VMDESTROY] = vmDestroy;

    result = Spawn("Start5", start5, NULL, 8*USLOSS_MIN_STACK, 2, &pid);
    if (result != 0) {
        console("start4(): Error spawning start5\n");
        Terminate(1);
    }
    result = Wait(&pid, &status);
    if (result != 0) {
        console("start4(): Error waiting for start5\n");
        Terminate(1);
    }
    Terminate(0);
    return 0; // not reached

} /* start4 */

/*
 *----------------------------------------------------------------------
 *
 * VmInit --
 *
 * Stub for the VmInit system call.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      VM system is initialized.
 *
 *----------------------------------------------------------------------
 */
static void
vmInit(sysargs *sysargsPtr)
{
    CheckMode();
	
	// verify valid arguments, arg4 = -1 if any illegal input
	if(sysargsPtr->arg1 != sysargsPtr->arg2 || (int)sysargsPtr->arg4 > MAXPAGERS){
		sysargsPtr->arg4 = (void *)-1;
		return;
	}
	// if the VM region has already been initialized, arg4 = -2
	if(vmInitialized == 1){
		sysargsPtr->arg4 = (void *)-2;
		return;
	}
	
	// call vmInitReal
	// place vm address in arg1 and returns
	sysargsPtr->arg1 = vmInitReal((int)sysargsPtr->arg1, (int)sysargsPtr->arg2,
							(int)sysargsPtr->arg3, (int)sysargsPtr->arg4);
		
	sysargsPtr->arg4 = (void *)0;
	
/*	// call vmInitReal
	vmAddress = vmInitReal((int)sysargsPtr->arg1, (int)sysargsPtr->arg2, (int)sysargsPtr->arg3,
							(int)sysargsPtr->arg4);
							
	// place vm address in arg1 and returns
	sysargsPtr->arg1 = vmAddress;	*/
	
	// set extern indicator variable to 1
	vmInitialized = 1;
	
	
} /* vmInit */


/*
 *----------------------------------------------------------------------
 *
 * vmDestroy --
 *
 * Stub for the VmDestroy system call.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      VM system is cleaned up.
 *
 *----------------------------------------------------------------------
 */

static void
vmDestroy(sysargs *sysargsPtr)
{
   CheckMode();
   
	// More to do here
	
	
	// no effect
	if(vmInitialized > 0){
		vmInitialized = 0;
	}
	else{
		vmDestroyReal();
	}
	
   
} /* vmDestroy */


/*
 *----------------------------------------------------------------------
 *
 * vmInitReal --
 *
 * Called by vmInit.
 * Initializes the VM system by configuring the MMU and setting
 * up the page tables.
 *
 * Results:
 *      Address of the VM region.
 *
 * Side effects:
 *      The MMU is initialized.
 *
 *----------------------------------------------------------------------
 */
void *
vmInitReal(int mappings, int pages, int frames, int pagers)
{
   int status;
   int dummy;
   int i;
   int sec;
   int track;
   int disk;
   char bufferName[50];

   CheckMode();
   clockHand = 0;
   status = USLOSS_MmuInit(mappings, pages, frames);
   if (status != USLOSS_MMU_OK) {
      USLOSS_Console("vmInitReal: couldn't init MMU, status %d\n", status);
      abort();
   }
   USLOSS_IntVec[USLOSS_MMU_INT] = FaultHandler;

   /*
    * Initialize page tables.
    */
	USLOSS_MmuInit(mappings, pages, frames);

   /* 
    * Create the fault mailbox.
    */
	for(i = 0; i < MAXPROC; i++){
		Process * temp = malloc(sizeof(Process));
		temp->numPages = 0;
		temp->pageTable = NULL;			  // create page table
		temp->procMbox = MboxCreate(0,0); // creates mailbox for fault resolution
		processes[i] = *temp;
	}
	frameTableSize = frames;
	
	frameTable = malloc(sizeof(FTE));
	frameTable->frame = 0;
	frameTable->next = NULL;
	frameTable->page = -1;
	frameTable->procNum = -1;
	frameTable->state = FRAME_UNUSED;
	
	// initialize frame table
	FTE *current = frameTable;
	FTE *temp;
	for(i = 1; i < frames; i++, current = current->next){
		temp = malloc(sizeof(FTE));
		temp->frame = i;
		temp->next = NULL;
		temp->page = -1;
		
		temp->status = FRAME_UNUSED;
		temp->procNum = -1;
	}
	
	// create disk occupancy table and calculate global disk params based on
	// MMU's page size
	int numTracks;
	int numSects;
	int sectSize;
	DiskSize(1 , &sectSize, &numSects, &numTracks);
	numBlocks = numTracks * numSects * sectSize / USLOSS_MmuPageSize();
	DBPerTrack = numBlocks / numTracks;
	diskBlocks = malloc(numBlocks * sizeof(int));
	sectsPerDB = (int)ceil(USLOSS_MmuPageSize() / sectSize);
	for(i = 0; i<numBlocks; i++){
		//diskBlocks[i] = 'DB unused'
	}
	
	// zero slot fault mailbox for fault objects to pass to pagers
	faultMailBox = MBoxCreate(MAXPROC, 0);
	
	// zero slot clock hand mailbox
	clockHandMbox = MboxCreate(1, 0);
	
	// disk block mailbox
	dBMbox = MboxCreate(1, 0);

   /*
    * Fork the pagers.
    */
	for(i = 0; i<MAXPAGERS; i++){
		sprintf(bufferName, "Pager %d", i+1);
		pagerHouses[i] = fork1(bufferName, Pager, NULL, USLOSS_MIN_STACK, PAGER_PRIORITY);
	}

   /*
    * Zero out, then initialize, the vmStats structure
    */
   memset((char *) &vmStats, 0, sizeof(VmStats));
   vmStats.pages = pages;
   vmStats.frames = frames;
   
 /*  if((i = DiskSize(1, &sec, &track, &disk)) != -1){
	   vmStats.diskBlocks = disk / USLOSS_MmuPageSize();
   }
   vmStats.freeDiskBlocks = vmStats.diskBlocks;
 */
// Not needed?

   /*
    * Initialize other vmStats fields.
    */
	vmStats.diskBlocks = diskBlocks;
	vmStats.freeFrames = frames;
	vmStats.switches = 0;
	vmStats.new = 0;
	vmStats.pageIns = 0;
	vmStats.pageOuts = 0;
	vmStats.replaced = 0;
	

   return USLOSS_MmuRegion(&dummy);
} /* vmInitReal */


/*
 *----------------------------------------------------------------------
 *
 * PrintStats --
 *
 *      Print out VM statistics.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      Stuff is printed to the USLOSS_Console.
 *
 *----------------------------------------------------------------------
 */
void
PrintStats(void)
{
     USLOSS_Console("VmStats\n");
     USLOSS_Console("pages:          %d\n", vmStats.pages);
     USLOSS_Console("frames:         %d\n", vmStats.frames);
     USLOSS_Console("diskBlocks:     %d\n", vmStats.diskBlocks);
     USLOSS_Console("freeFrames:     %d\n", vmStats.freeFrames);
     USLOSS_Console("freeDiskBlocks: %d\n", vmStats.freeDiskBlocks);
     USLOSS_Console("switches:       %d\n", vmStats.switches);
     USLOSS_Console("faults:         %d\n", vmStats.faults);
     USLOSS_Console("new:            %d\n", vmStats.new);
     USLOSS_Console("pageIns:        %d\n", vmStats.pageIns);
     USLOSS_Console("pageOuts:       %d\n", vmStats.pageOuts);
     USLOSS_Console("replaced:       %d\n", vmStats.replaced);
} /* PrintStats */


/*
 *----------------------------------------------------------------------
 *
 * vmDestroyReal --
 *
 * Called by vmDestroy.
 * Frees all of the global data structures
 *
 * Results:
 *      None
 *
 * Side effects:
 *      The MMU is turned off.
 *
 *----------------------------------------------------------------------
 */
void
vmDestroyReal(void)
{

   CheckMode();
   USLOSS_MmuDone();
   
   /*
    * Kill the pagers here.
    */
	char * dummy;
	int i;
	for(i = 0; i<MAXPAGERS; i++){
		MboxCondSend(faultMailBox, dummy, 0);
		zap(pagerPIDS[i]);
	}
	
   /* 
    * Print vm statistics.
    */
   console("vmStats:\n");
   console("pages: %d\n", vmStats.pages);
   console("frames: %d\n", vmStats.frames);
   console("blocks: %d\n", vmStats.blocks);
   /* and so on... */
   
   // free malloc'd stuff
   free(frameTable);

} /* vmDestroyReal */

/*
 *----------------------------------------------------------------------
 *
 * FaultHandler
 *
 * Handles an MMU interrupt. Simply stores information about the
 * fault in a queue, wakes a waiting pager, and blocks until
 * the fault has been handled.
 *
 * Results:
 * None.
 *
 * Side effects:
 * The current process is blocked until the fault is handled.
 *
 *----------------------------------------------------------------------
 */
static void
FaultHandler(int  type /* USLOSS_MMU_INT */,
             void *arg  /* Offset within VM region */)
{
   int cause;

   int offset = (int) (long) arg;

   assert(type == USLOSS_MMU_INT);
   cause = USLOSS_MmuGetCause();
   assert(cause == USLOSS_MMU_FAULT);
   vmStats.faults++;
   
   /*
    * Fill in faults[pid % MAXPROC], send it to the pagers, and wait for the
    * reply.
    */
	faults[getpid() % MAXPROC].addr = arg;
	faults[getpid() % MAXPROC].pid = getpid();
	
	// send with the blocks on the process's mbox until the fault is resolved
	MboxSend(faultMailBox, faults[getpid() % MAXPROC].addr, sizeof(void*));
} /* FaultHandler */

/*
 *----------------------------------------------------------------------
 *
 * Pager --
 *
 * Kernel process that handles page faults and does page replacement.
 *
 * Results:
 * None.
 *
 * Side effects:
 * None.
 *
 *----------------------------------------------------------------------
 */
static int
Pager(char *buf)
{
	FaultMsg * faultObj = malloc(sizeof(struct FaultMsg));
	int iter;			// iterator values
	int freeFrame;		// index of free frame;
	char* pageBuff;		// buffer for page that has been written and needs to be
						// transferred to disk
	char * dummy;		// dummy buffer for mbox operations
	PTE * pgPtr;		// pointer to page table entry for unoccupied frame
	int axBits;			// access bits for a particular page
	int pageNum;		// page number for use in page table
	int * numPgsPtr;
	
	pageBuff = malloc(USLOSS_MmuPageSize()); // allocate page buffer and zero out
	
    while(1) {
        /* Wait for fault to occur (receive from mailbox) */
        /* Look for free frame */
        /* If there isn't one then use clock algorithm to
         * replace a page (perhaps write to disk) */
        /* Load page into frame from disk, if necessary */
        /* Unblock waiting (faulting) process */
		MboxRecieve(faultMailBox, faultObj, sizeof(void *));
		
		if(pagerkill){
			free(faultObj);
			free(pageBuff);
			break;
		}
		
		pgPtr = geFtPageTableEntry(processes[faultObj->pid % MAXPROC].pageTable, pageNum);
		
		for(iter = 0, freeFrame = -1; iter<frameTableSize; iter++){
			if(frameTable[iter].state == DATA_UNUSED){
				freeFrame = iter;
				break;
			}
		}
		
		// if a free frame is found, update page table entry for the process within
		// the free frame's pointer
		if(freeFrame >= 0){
			pgPtr->frame = freeFrame;
			pgPtr->state = INCORE;
			if(pgPtr == INDISK){
				pageDiskFetch(pgPtr->diskBlock, pgPtr->page);
			}
			frameTable[freeFrame].state = FRAME_INUSE;
		}
		// use clock algorithm to replace page within frame table
		// (maybe write to disk?)
		else{
			for(;;clockHand = ++clockHand % frameTableSize){
				
				// enter clock hand mutex to check bits and maybe do assignment
				MboxSend(clockHandMbox, dummy, 0);
				
				// retrieve use bits to check if recently referenced
				USLOSS_MmuGetAccess(clockHand, &axBits);
				
				//if referenced, change marking to zero and continue
				if(axBits & USLOSS_MMU_REF){
					axBits = axBits & USLOSS_MMU_DIRTY;
					USLOSS_MmuSetAccess(clockHand, axBits);
				}
				else{
					// find page's entry in process's page table
					// create if not found
					pageNum = (int)faultObj->addr / USLOSS_MmuPageSize();
					
					// if frame is dirty move page contents to temporary buffer to be
					// written to disk
					if(axbits & USLOSS_MMU_DIRTY){
						memcpy(pageBuff, frameTable[clockHand].page + USLOSS_MmuRegion(numPgsPtr), USLOSS_MmuPageSize();
						pgPtr->diskBlock = pageDiskWrite(pageBuff);
						//pgPtr->state = 'in disk';
					}
					
					// if page has info stored on disk, write to memory
					if(pgPtr->state == DATA_INUSE){
						pageDiskFetch(pgPtr->diskBlock, pgPtr->page);
					}
				}
				// exit clock hand mutex
				MboxCondRecieve(clockHandMbox, dummy, 0);
			}
		}
		// unblock waiting process
		MboxSend(faultObj->replyMbox, buf, 0);
    }
    return 0;
} /* Pager */

/*
 *----------------------------------------------------------------------
 *
 * getPageTableEntry --
 *
 * Gets the page table entry from the process's page table.
 *
 * Results:
 * The page table entry from the process's page table.
 *
 * Side effects:
 * If there is no page table entry found, one is created.
 *
 *----------------------------------------------------------------------
 */
PTE * getPageTableEntry(PTE * head, int, pgNum){
	PTE * target = head;
	
	for(; target != NULL; target = target->nextPage){
		// if page entry is found, return it
		if(target->page == pgNum){
			break;
		}
		else if(target->nextPage == NULL){
			// page didn't have an entry so create one
			target->nextPage = malloc(sizeof(PTE));
			target = target->nextPage;
			target->page = pgNum;
			target->diskBlock = -1;
			break;
		}
	}
	return target;	
}/* getPageTableEntry */

/*
 *----------------------------------------------------------------------
 *
 * writePageDisk --
 *
 * Writes the contents ofa  page to disk
 *
 * Results:
 * Datablock it was written to.
 *
 * Side effects:
 * Contents written to disk.
 *
 *----------------------------------------------------------------------
 */
 int writePageDisk(char *pageBuff){
	 int dBl
	 int track;
	 int sector;
	 int status;
	 char *dummy;
	 
	 // Enter the mmu diskwrite
	 MboxSend(dBMboxm dummy, 0);
	 
	 // Find free datablock and mark to read
	 for(dB = 0; dB<diskBlocks && diskBlocks[dB] == DATA_INUSE; dB++);
	 diskBlocks[dB] = DATA_INUSE;
	 
	 // diskblock to track and sectors
	 track = dB/DBPerTrack;
	 sector = dB/sectsPerDB - (track * USLOSS_DISK_TRACK_SIZE);
	 
	 // write info to the disk
	 DiskWrite(pageBuff, 1, track, sector, USLOSS_MmuPageSize() / USLOSS_DISK_SECTOR_SIZE, &status);
	 
	 return dB;
 }/* writePageDisk */

