//
//  sems.h
//  Phase3
//
//  Created by Sean Vaeth on 10/15/16.
//  Copyright © 2016 Sean Vaeth. All rights reserved.
//

#ifndef sems_h
#define sems_h

typedef struct procStruct procStruct;
typedef procStruct * procPtr;

typedef struct semaphore semaphore;
typedef semaphore * semPtr;

struct procStruct{
    int     pid;
    int     status;
    int     mboxID;
    int     (*func)(char *);
    char    *name;
    char    *arg;
    semPtr  sem;
    procPtr childProc;
    procPtr nextSiblingProc;
    procPtr parentPtr;
    procPtr nextBlockedPtr;
};

struct semaphore{
    int count;
    int semID;
    procPtr blockedPtr;
};

#define DEBUG3 1

#define MAXSPAWNS MAXPROC - 3

#define SYSCALLS     10
#define UNUSED      -1
#define USED         1

#endif /* sems_h */
