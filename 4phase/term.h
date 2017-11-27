//
//  Term.h
//  phase4
//
//  Created by Sean Vaeth on 11/10/16.
//  Copyright © 2016 Sean Vaeth. All rights reserved.
//

#ifndef Term_h
#define Term_h

#include <stdio.h>

extern int TermDriver(char *);
extern int TermReader(char *);
extern int TermWriter(char *);
extern void termRead(systemArgs *);
extern void termWrite(systemArgs *);
extern int termReadReal(int, int ,char *);
extern int termWriteReal(char *, int, int, int*);

#endif /* Term_h */
