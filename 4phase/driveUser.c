//
//  driveUser.c
//  phase4
//
//  Created by Sean Vaeth on 11/4/16.
//  Copyright © 2016 Sean Vaeth. All rights reserved.
//

#include <phase4.h>
#include <phase2.h>
#include <usloss.h>
#include <usyscall.h>


#define CHECKMODE {    \
if (USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE) { \
    USLOSS_Console("Trying to invoke syscall from kernel\n"); \
    USLOSS_Halt(1);  \
    }  \
}

int  Sleep(int seconds){
    systemArgs sysArg;
    
    CHECKMODE;
    sysArg.number = SYS_SLEEP;
    sysArg.arg1 = (void *) (long) seconds;
    
    USLOSS_Syscall(&sysArg);
    
    return (int) (long) sysArg.arg4;
}

int  DiskRead (void *diskBuffer, int unit, int track, int first,
               int sectors, int *status){
    systemArgs sysArg;
    CHECKMODE;
    sysArg.number = SYS_DISKREAD;
    sysArg.arg1 = diskBuffer;
    sysArg.arg2 = (void *) (long) sectors;
    sysArg.arg3 = (void *) (long) track;
    sysArg.arg4 = (void *) (long) first;
    sysArg.arg5 = (void *) (long) unit;
    
    USLOSS_Syscall(&sysArg);
    
    *status = (int) (long) sysArg.arg1;
    return (int) (long) sysArg.arg4;
}

int  DiskWrite(void *diskBuffer, int unit, int track, int first,
               int sectors, int *status){
    systemArgs sysArg;
    CHECKMODE;
    sysArg.number = SYS_DISKWRITE;
    sysArg.arg1 = diskBuffer;
    sysArg.arg2 = (void *) (long) sectors;
    sysArg.arg3 = (void *) (long) track;
    sysArg.arg4 = (void *) (long) first;
    sysArg.arg5 = (void *) (long) unit;
    
    USLOSS_Syscall(&sysArg);
    
    *status = (int) (long) sysArg.arg1;
    return (int) (long) sysArg.arg4;
}

int  DiskSize (int unit, int *sector, int *track, int *disk){
    systemArgs sysArg;
    CHECKMODE;
    sysArg.number = SYS_DISKSIZE;
    sysArg.arg1 = (void *) (long) unit;
    
    USLOSS_Syscall(&sysArg);
    
    *sector = (int) (long) sysArg.arg1;
    *track = (int) (long) sysArg.arg2;
    *disk = (int) (long) sysArg.arg3;
    return (int) (long) sysArg.arg4;
}

int  TermRead (char *buffer, int bufferSize, int unitID,
               int *numCharsRead){
    systemArgs sysArg;
    CHECKMODE;
    sysArg.number = SYS_TERMREAD;
    sysArg.arg1 = (void *) buffer;
    sysArg.arg2 = (void *) (long) bufferSize;
    sysArg.arg3 = (void *) (long) unitID;
    
    USLOSS_Syscall(&sysArg);

    *numCharsRead = (int) (long) sysArg.arg2;
    return (int) (long) sysArg.arg4;
}

int  TermWrite(char *buffer, int bufferSize, int unitID,
               int *numCharsRead){
    systemArgs sysArg;
    CHECKMODE;
    sysArg.number = SYS_TERMWRITE;
    sysArg.number = SYS_TERMREAD;
    sysArg.arg1 = (void *) buffer;
    sysArg.arg2 = (void *) (long) bufferSize;
    sysArg.arg3 = (void *) (long) unitID;
    
    USLOSS_Syscall(&sysArg);
    
    *numCharsRead = (int) (long) sysArg.arg2;
    return (int) (long) sysArg.arg4;
}

