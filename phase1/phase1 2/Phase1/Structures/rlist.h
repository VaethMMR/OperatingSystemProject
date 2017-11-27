#ifndef RLIST_H
#define RLIST_H
#include "queue.h"

typedef struct {
	Queue **lists;
	int count;
	int levels;
} RList;

int  rlistCreate(int);
int  rlistAdd(QData);
int  rlistPop(int, QData *);
int  rlistRemove(int, QData *);
void rlistDestroy(void);
void rlistPrint(void);

#endif
