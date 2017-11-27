
#define DEBUG2 1

typedef struct mailSlot *slotPtr;
typedef struct mailSlot  mailSlot;
typedef struct mailbox   mailbox;
typedef struct mailbox *mboxPtr;
typedef struct mboxProc *mboxProcPtr;
typedef struct procStruct procStruct;
typedef struct mboxProc mboxProc;
typedef procStruct *procPtr;

struct mailbox {
    int       mboxID;
    int       numSlots;
    int       slotSize;
    int       activeSlots;
    int       pid;
    int       status;
    slotPtr   slotPtr;
    mboxProcPtr sendPtr;
    mboxProcPtr receivePtr;
    // other items as needed...
};

struct mailSlot {
    int       mboxID;
    int       status;
    int       msgSize;
    int       slotID;
    char      message[MAX_MESSAGE];
    slotPtr   nextSlotPtr;
    // other items as needed...
};

struct mboxProc {
    int       pid;
    int       status;
    char      message[MAX_MESSAGE];
    int       msgSize;
    mboxProcPtr nextSendPtr;
    mboxProcPtr nextReceivePtr;
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