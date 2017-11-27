/*
 *
 * vm.h
 *
 */

/*
 * Tag for processes
 */
//#define TAG 0
// NOTE: Not Needed

/*
 * States for pages
 */
#define UNUSED 500
#define INCORE 501
#define INDISK 502
#define BOTH 503
#define OPEN -1;
#define CLOSED 1;
// NOTE MAY NEED TO CHANGE THESE
 
/*
 * States for datablocks
 */
#define DATA_UNUSED 1401
#define DATA_INUSE 1402;
// NOTE MAY NEED TO CHANGE THESE
 
/*
 * States for frames
 */
#define FRAME_UNUSED 1501
#define FRAME_INUSE 1502
// NOTE MAY NEED TO CHANGE THESE 

// Check the mode
#define CheckMode() assert(USLOSS_PserGet() & USLOSS_PSR_CURRENT_MODE)

/*
 * Page table entry
 */
typedef struct PTE{
	int state;				// State defined from above
	int page;				// Page offset in memory, -1 if none
	int frame;				// Frame that stores the page, -1 if none
	int diskBlock;			// Disk block that stores the page, -1 if none
	
	struct PTE *nextPage;	// Next page table entry
} PTE;



/*
 * Frame Table Entry
 */
typedef struct FTE{
	int frame;				// Frame number reference
	int page;				// Page that references this frame, -1 if none
	int procNum;			// Number of process that owns the frame
	int state;				// State defined from above
	
	struct FTE *next;		// next frame table entry
} FTE;


/*
 * Per-process Info
 */
typedef struct Process{
	int numPages;		// Size of page table
	PTE *pageTable;		// Page table for the process
	
	int procMbox;		// Mailbox which the proc an wait for fault resolution
} Process;
 
/*
 * Page fault info.  Message sent by faulting process to pager
 * to request the fault be handled.
 */
typedef struct FaultMsg{
	int pid;			// PID of fault process
	void *addr;			// Address of page that caused the fault
	int replyMbox;		// Mbox to send reply
} FaultMsg;
