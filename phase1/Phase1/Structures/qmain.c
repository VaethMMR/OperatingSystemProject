#include <stdio.h>
#include "queue.h"

int main(){
	int i, j;
	Queue *list[6];
	printf("TEST CREATE/ENQUEUE:\n");
	for(i = 0 ; i < 6; i++){
		list[i] = queueCreate(i);
		for(j = i; j < 6; j++){
			QData in;
			in.pid = j*100;
			enqueue(list[i], in);
		}
		queuePrint(list[i]);
	}

	printf("TEST DEQUEUE:\n");
	for(i = 0; i < 6; i++){
		QData pop;
		dequeue(list[i], &pop);
		queuePrint(list[i]);
	}

	printf("TEST REMOVE 300, 500:\n");
	for(i = 0; i < 6; i++){
		QData ret;
		queueRemove(list[i], 300, &ret);
		queueRemove(list[i], 500, &ret);
		queuePrint(list[i]);
	}

	for(i = 0; i < 6; i++){
		queueDestroy(list[i]);
	}
	return 0;
}
