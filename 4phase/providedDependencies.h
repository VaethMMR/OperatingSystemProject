//
//  providedStructs.h
//  phase4
//
//  Created by Sean Vaeth on 11/10/16.
//  Copyright © 2016 Sean Vaeth. All rights reserved.
//

#ifndef providedStructs_h
#define providedStructs_h

typedef struct procStruct procStruct;
typedef procStruct * procPtr;

typedef struct semaphore semaphore;
typedef semaphore * semPtr;

struct semaphore{
    int count;
    int semID;
    procPtr blockedPtr;
};

struct procStruct{
    // shared
    int         pid;
    procPtr     nextProcPtr;
    semaphore   sem;
    
    // clock
    int         wakeupTime;
    
    // disk
    int         sectors;
    int         first;
    int         track;
    int         rwFlag;
    void       *buffer;
    
    // terminal
    procPtr     driver;
    procPtr     reader;
    procPtr     writer;
    
    // term reader
    int         readerMbox;
    int         receiverMbox;
    
    // term writer
    int         writerMbox;
};

void checkKernel(char *);

extern procStruct Proc4Table[];

semaphore running;

#define DEBUG4 1

#define MICROS  1000000

#define READ 0
#define WRITE 1

#endif /* providedStructs_h */
