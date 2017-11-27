//
//  Disk.h
//  phase4
//
//  Created by Sean Vaeth on 11/10/16.
//  Copyright © 2016 Sean Vaeth. All rights reserved.
//

#ifndef Disk_h
#define Disk_h

#include <stdio.h>

extern int DiskDriver(char *);
extern void diskWrite(systemArgs *);
extern void diskRead (systemArgs *);
extern int diskReadReal(int, int, int, int, void*);
extern int diskWriteReal(int, int, int, int, void*);
extern void diskSize(systemArgs *);
extern int diskSizeReal(int, int*, int*, int*);

#define LEFT 0
#define RIGHT 1

#endif /* Disk_h */
