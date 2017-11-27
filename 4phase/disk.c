//
//  Disk.c
//  phase4
//
//  Created by Sean Vaeth on 11/10/16.
//  Copyright © 2016 Sean Vaeth, Brandon Wong. All rights reserved.
//

#include <usloss.h>
#include <phase1.h>
#include <phase2.h>
#include <stdlib.h>

#include "providedPrototypes.h"
#include "providedDependencies.h"
#include "disk.h"

// ====================PROTOTYPES=======================
int DiskDriver(char *);
extern void checkKernel(char *);

void insertDiskReq(procPtr, int);
void dequeueDiskReq(int);
void printDiskList(int);

// ====================GLOBALS==========================
extern procStruct Proc4Table[];
extern semaphore running;
extern semaphore diskLock;
int arm = 0;
int sizes[2];
int diskPIDS[USLOSS_DISK_UNITS];
procPtr RightList = NULL;
procPtr LeftList = NULL;

// ====================DEBUG FLAGS==========================
int debugDisk = 0;
int debugDiskRead = 0;
int debugDiskWrite = 0;
int debugDiskSize = 0;
static int debugQueue = 0;

/*
 *  Routine:  DiskDriver
 *
 *  Description: Driver for two disk devices.  Requests are made via DiskWrite,
 *				 DiskRead, or DiskSize.  Read and write requests are optimized for
 *				 seek by using circular scan for the requests on the disk.
 *
 *  Arguments: args -- the unit of the disk.
 *
 *  Return: none
 *  side effect: determines if the transfer was successful and if illegal values were given
 *				 as input.
 */
int DiskDriver(char *arg){
    int i;
    int diskUnit = atoi(arg);
    int status, size;
    procPtr cur = &Proc4Table[getpid() % MAXPROC];
    procPtr ptr;
    USLOSS_DeviceRequest req;
    req.opr = USLOSS_DISK_TRACKS;
    req.reg1 = &size;
    
    //save the pid of the drivers
    diskPIDS[diskUnit] = getpid();

    //get size of disk
    USLOSS_DeviceOutput(USLOSS_DISK_DEV, diskUnit, &req);
    waitDevice(USLOSS_DISK_DEV, diskUnit, &status);

    sizes[diskUnit] = size;

    // Let the parent know we are running and enable interrupts.
    semVReal(running.semID);
    USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);

    while(! isZapped()){
        // block on semaphore until request to process
        semPReal(cur->sem.semID);
        
        // dequeue req from queue
        if(RightList != NULL || LeftList != NULL){
            semPReal(diskLock.semID);
            if(RightList != NULL){
                ptr = RightList;
                dequeueDiskReq(RIGHT);
            }
            else{
                ptr = LeftList;
                dequeueDiskReq(LEFT);
            }
            semVReal(diskLock.semID);
            
            // move arm to the track
            req.opr = USLOSS_DISK_SEEK;
            req.reg1 = (void *) (long) ptr->track;
            
            USLOSS_DeviceOutput(USLOSS_DISK_DEV, diskUnit, &req);
	    waitDevice(USLOSS_DISK_DEV, diskUnit, &status);
            arm = ptr->track;
            
            // evaluate over the sectors
	    int sector = ptr->first;
	    int tracks = 0;
            for(i = 0; i < ptr->sectors; i++){
		sector = ptr->first + i;
		if(sector >= USLOSS_DISK_TRACK_SIZE){
		    tracks++;
		    sector = sector - 16 * tracks;
		    req.opr = USLOSS_DISK_SEEK;
		    req.reg1 = (void *) (long) ptr->track;
		    USLOSS_DeviceOutput(USLOSS_DISK_DEV, diskUnit, &req);
		    waitDevice(USLOSS_DISK_DEV, diskUnit, &status);
		}
		
                req.reg1 = (void *) (long) sector;
		req.reg2 = ptr->buffer;
                if(ptr->rwFlag == READ){
                    req.opr = USLOSS_DISK_READ;
                }
                else{
                    req.opr = USLOSS_DISK_WRITE;
                }
                USLOSS_DeviceOutput(USLOSS_DISK_DEV, diskUnit, &req);
                waitDevice(USLOSS_DISK_DEV, diskUnit, &status);
            }
            
            // wakeup the process, send back data
            semVReal(ptr->sem.semID);
        }
    }
    return 0;
}/* DiskDriver */

/*
 *  Routine:  diskRead
 *
 *  Description: reads sectors from the disk indicated by the unit and starts
 *				 at track and the first sector.  The driver must handle a range
 *				 of sectors specified by first and sectors that spans a track
 *				 boundary.  A file cannot wrap around the end of the disk.
 *
 *  Arguments: args -- a system argument struct containing information to be used in
 *					   diskReadReal
 *
 *  Return: none
 *  side effect: determines if the transfer was successful and if illegal values were given
 *				 as input.
*/
void diskRead (systemArgs *args){
	
	void * bufferAddress = args->arg1;
	int sectorsNum = (int) (long) args->arg2;
	int trackNum = (int) (long) args->arg3;
	int firstNum = (int) (long) args->arg4;
	int unitNum = (int) (long) args->arg5;
	
	int readNum = diskReadReal(unitNum, trackNum, firstNum, sectorsNum, bufferAddress);
	
	if(readNum == 0){
		args->arg1 = 0;
		args->arg4 = 0;
	}
	else{
		args->arg1 = bufferAddress;
		args->arg4 = (void *) (long) -1;
	}
	
}/* diskRead */

/*
 *  Routine:  diskReadReal
 *
 *  Description: reads sectors from the disk indicated by the unit and starts
 *				 at track and the first sector.  The driver must handle a range
 *				 of sectors specified by first and sectors that spans a track
 *				 boundary.  A file cannot wrap around the end of the disk.
 *
 *  Arguments: unit -- disk that is indicated
 *			   track -- track on the disk
 *			   first -- the first sector of the disk
 *			   sectors -- number of sectors
 *			   buffer -- address to be read
 *
 *  Return: -1 if there are invalid parameters, 0 if sectors read successfully, >0 status register
 *  side effect: none
*/
int diskReadReal(int unit, int track, int first, int sectors, void* buffer){
    procPtr cur = &Proc4Table[getpid() % MAXPROC];
    procPtr driver = &Proc4Table[diskPIDS[unit] % MAXPROC];
    if(unit < 0 || unit >= USLOSS_DISK_UNITS || sectors < first || first < 0 || sectors < 0 || track < 0)
        return -1;
    
    // set up disk proc
    cur->pid = getpid();
    cur->sectors = sectors;
    cur->first = first;
    cur->track = track;
    cur->buffer = buffer;
    cur->rwFlag = READ;
    
    // put disk req on list
    // if the arm is left of the track put on right list
    // if the arm is right of the track put on left list
    // if the arm is on the track put on the right list
    if(arm <= cur->track){
        insertDiskReq(cur, RIGHT);
    }
    else{
        insertDiskReq(cur, LEFT);
    }
    
    // wakeup the driver
    semVReal(driver->sem.semID);
    
    // block on private sem
    semPReal(cur->sem.semID);
    
	return 0;
}/* diskReadReal */

/*
 *  Routine:  diskWrite
 *
 *  Description: writes sectors to the disk indicated by the unit and starts
 *				 at track and the first sector.  The driver must handle a range
 *				 of sectors specified by first and sectors that spans a track
 *				 boundary.  A file cannot wrap around the end of the disk.
 *
 *  Arguments: args -- a system argument struct containing information to be used in
 *					   diskWriteReal
 *
 *  Return: none
 *  side effect: determines if the transfer was successful and if illegal values were given
 *				 as input.
*/
void diskWrite(systemArgs *args){
	void * bufferAddress = args->arg1;
	int sectorsNum = (int) (long) args->arg2;
	int trackNum = (int) (long) args->arg3;
	int firstNum = (int) (long) args->arg4;
	int unitNum = (int) (long) args->arg5;
	
	int writeNum = diskReadReal(unitNum, trackNum, firstNum, sectorsNum, bufferAddress);
	
	if(writeNum == 0){
		args->arg1 = 0;
		args->arg4 = 0;
	}
	else{
		args->arg1 = bufferAddress;
		args->arg4 = (void *) (long) -1;
	}
}/* diskWrite */

/*
 *  Routine:  diskWriteReal
 *
 *  Description: writes sectors to the disk indicated by the unit and starts
 *				 at track and the first sector.  The driver must handle a range
 *				 of sectors specified by first and sectors that spans a track
 *				 boundary.  A file cannot wrap around the end of the disk.
 *
 *  Arguments: unit -- disk that is indicated
 *			   track -- track on the disk
 *			   first -- the first sector of the disk
 *			   sectors -- number of sectors
 *			   buffer -- to be written into
 *
 *  Return: -1 if there are invalid parameters, 0 if sectors read successfully, >0 status register
 *  side effect: none
*/
int diskWriteReal(int unit, int track, int first, int sectors, void* buffer){
    procPtr cur = &Proc4Table[getpid() % MAXPROC];
    procPtr driver = &Proc4Table[diskPIDS[unit] % MAXPROC];
	if(unit < 0 || unit >= USLOSS_DISK_UNITS || sectors < first || first < 0 || sectors < 0 || track < 0)
		return -1;
	
    //set up disk proc
    cur->pid = getpid();
    cur->sectors = sectors;
    cur->first = first;
    cur->track = track;
    cur->buffer = buffer;
    cur->rwFlag = WRITE;
    
    //put disk req on list
    // if the arm is left of the track put on right list
    // if the arm is right of the track put on left list
    // if the arm is on the track put on the right list
    
    //access restriction to the disk queue
    semPReal(diskLock.semID);
    if(arm <= cur->track){
        insertDiskReq(cur, RIGHT);
    }
    else{
        insertDiskReq(cur, LEFT);
    }
    semVReal(diskLock.semID);
    
    semVReal(driver->sem.semID);
    
    // block on private sem
    semPReal(cur->sem.semID);
	
	return 0;
}/* diskWriteReal */

/*
 *  Routine:  diskSizeReal
 *
 *  Description: Returns information about the size of the disk.
 *				 
 *
 *  Arguments: args -- a system argument struct containing information to be used in
 *					   diskSizeReal
 *  Return: none
 *  side effect: outputs the size of a sector (in bytes), number of sectors in a track,
 *  			 number of tracks in the disk, and determines if illegal values were given
 *				 as input.
*/
void diskSize(systemArgs *args){
	
    int unitNum = (int) (long) args->arg1;
    int sector, track, disk;
	
    int diskResult = diskSizeReal(unitNum, &sector, &track, &disk);
	
    if(diskResult == -1){
		args->arg4 = (void *) (long) diskResult;
    }
    else{
       	args->arg1 = (void *) (long) sector;
       	args->arg2 = (void *) (long) track;
     	args->arg3 = (void *) (long) disk;
	args->arg4 = 0;
    }
}/* diskSize */

/*
 *  Routine:  diskSizeReal
 *
 *  Description: Returns information about the size of the disk.
 *				 
 *
 *  Arguments: unit -- disk that is indicated
 *			   sector -- filled in with the number of bytes in a sector
 *			   track -- number of sectors in a track
 *			   disk -- number of sectors
 * 
 *  Return: -1 if there are invalid parameters, 0 if disk size parameters read successfully
 *  side effect: none
*/
int diskSizeReal(int unit, int *sector, int *track, int *disk){
	
    if(unit < 0 || unit > 1){
        return -1;
    }
    
    *sector = USLOSS_DISK_SECTOR_SIZE;
    *track = USLOSS_DISK_TRACK_SIZE;
    *disk = sizes[unit];
	
	return 0;
}/* diskSizeReal */

/* ------------------------------------------------------------------------
 Name - insertDiskReq
 Purpose - Enqueues a running process to the ready list.
 Parameters - The running process to be enqueued to the ready list.
 Returns - none
 Side Effects - Puts the process onto the ready list and gives it a priority and sets
 the status to READY.
 ------------------------------------------------------------------------ */
void insertDiskReq(procPtr ptr, int list){
    procPtr cur = NULL;
    procPtr *DiskList = list == RIGHT ? &RightList : &LeftList;
    if(ptr != NULL){
        if(*DiskList == NULL){
            *DiskList = ptr;
        }
        else{
            for(cur = *DiskList; cur->nextProcPtr != NULL && cur->nextProcPtr->track < cur->track; cur = cur->nextProcPtr);
            ptr->nextProcPtr = cur->nextProcPtr;
            cur->nextProcPtr = ptr;
        }
    }
    if(DEBUG4 && debugQueue){
        USLOSS_Console(ptr == NULL ? "NULL\n" : "Enqueue %d to list\n", ptr->pid);
        printDiskList(list);
    }
}/* insertDiskReq */

/* ------------------------------------------------------------------------
 Name - dequeueDiskReq
 Purpose - Dequeue a process from the ready list
 Parameters - The next process to be dequeued from the ready list.
 Returns - none
 Side Effects -  The process is dequeued from the ready list.
 ------------------------------------------------------------------------ */
void dequeueDiskReq(int list){
    procPtr *DiskList = list == RIGHT ? &RightList : &LeftList;
    procPtr cur = *DiskList;
    if(cur != NULL){
        *DiskList = (*DiskList)->nextProcPtr;
        cur->nextProcPtr = NULL;
    }
    if(DEBUG4 && debugQueue){
        USLOSS_Console( cur == NULL ? "NULL\n" : "Dequeue %d from list\n", cur->pid);
        printDiskList(list);
    }
}/* dequeueDiskReq */

/* ------------------------------------------------------------------------
 Name - printDiskList
 Purpose - Used for debugging to print out the ready list.
 Parameters - none
 Returns - none
 Side Effects - none
 ------------------------------------------------------------------------ */
void printDiskList(int list){
    USLOSS_Console("********* DISK LIST **********\n");
    procPtr *DiskList = list == RIGHT ? &RightList : &LeftList;
    procPtr cur = *DiskList;
    USLOSS_Console("Disk: ");
    while(cur != NULL){
        USLOSS_Console("|%d|", cur->pid);
        USLOSS_Console("->");
        cur = cur -> nextProcPtr;
    }
    USLOSS_Console("NULL\n");
    
    USLOSS_Console("********* DISK LIST **********\n");
}/* printDiskList */

