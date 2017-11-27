/* Patrick's DEBUG printing constant... */
#define DEBUG 1

typedef struct procStruct procStruct;

typedef struct procStruct * procPtr;

struct procStruct {
    procPtr         nextProcPtr;
    procPtr         childProcPtr;
    procPtr         nextSiblingPtr;
    procPtr         nextDeadChild;
    procPtr         nextDeadSibling;
    procPtr         nextZombieChild;
    procPtr         nextZombieSibling;
    procPtr         zapPtr;
    procPtr         nextZapPtr;
    procPtr         parentPtr;
    
    char            name[MAXNAME];     /* process's name */
    char            startArg[MAXARG];  /* args passed to process */
    
    USLOSS_Context  state;             /* current context for process */
    char           *stack;
    unsigned int    stackSize;
    
    short           pid;               /* process id */
    short           ppid;                /* parent process id */
    
    int (* startFunc) (char *);   /* function where process begins -- launch */
    
    int             priority;
    int             status;        /* READY, BLOCKED, QUIT, etc. */
    int             numChildren;
    int		    tableChildren;
    int             numActiveChildren;
    int             numDeadChildren;
    int             numZombieChildren;
    int             numJoinedChildren;
    int             tableSlot;
    int             isZapped;
    int             timeslice;
    int             startTime;
    int             sliceCount;
    int             deadStatus;
};

struct psrBits {
    unsigned int curMode:1;
    unsigned int curIntEnable:1;
    unsigned int prevMode:1;
    unsigned int prevIntEnable:1;
    unsigned int unused:28;
};

union psrValues {
   struct psrBits bits;
   unsigned int integerPart;
};

/* Some useful constants.  Add more as needed... */
#define NO_CURRENT_PROCESS NULL
#define MINPRIORITY 5
#define MAXPRIORITY 1
#define SENTINELPID 1
#define SENTINELPRIORITY (MINPRIORITY + 1)

#define TRUE 1
#define FALSE 0

#define TIMESLICE 80000

#define UNUSED 0
#define READY 1
#define RUNNING 2
#define QUIT 3
#define JOINBLOCKED 4
#define ZAPBLOCKED 5
#define BLOCKMEBLOCKED 11
