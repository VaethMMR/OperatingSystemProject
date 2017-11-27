#include <stdio.h>
#include <stdlib.h>
#include "rlist.h"

#define SIZE 6
#define MAX 50

int main(){
	int i;
	rlistPrint();
	printf("TEST CREATE\n");
	rlistCreate(SIZE);
	rlistPrint();
	
	printf("TEST ADD\n");
	for(i = 0; i < MAX; i++){
		QData in;
		in.pid = i * 100;
		in.priority = rand() % SIZE;
		printf("ADD %d to %d: %d\n", in.pid, in.priority, rlistAdd(in));
	}
	rlistPrint();

	printf("TEST DEQUEUE\n");
	for(i = 0; i < SIZE; i++){
		QData ret;
		printf("DEQUEUE %d: ", i);
		printf("%d\n", rlistPop(i, &ret));
	}
	rlistPrint();
	
	printf("TEST REMOVE\n");
	QData ret;
	printf("REMOVE 300: ");
	printf("%d ", rlistRemove(300, &ret));
	printf("level %d pid %d\n", ret.priority, ret.pid);
	rlistPrint();
	printf("REMOVE 1600: ");
	printf("%d ", rlistRemove(1600, &ret));
	printf("lever %d pid %d\n", ret.priority, ret.pid);
	rlistPrint();
	printf("REMOVE 4800: ");
	printf(" %d ", rlistRemove(4800, &ret));
	printf("level %d pid %d\n", ret.priority, ret.pid);
	rlistPrint();
	
	printf("TEST DESTROY\n");
	rlistDestroy();
	rlistPrint();
	return 0;
}
