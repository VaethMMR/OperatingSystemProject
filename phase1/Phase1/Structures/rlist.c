#include <stdio.h>
#include <stdlib.h>
#include "rlist.h"
#include "queue.h"

RList *list = NULL;

int rlistCreate(int size){
	int i;
	if(list != NULL){
		printf("rlist.c: can only have one rlist at a time\n");
		return 0;
	}
	if((list = malloc(sizeof(RList))) == NULL){
		printf("rlist.c: Could not allocate ready list\n");
		return 0;
	}

	if((list->lists = malloc(sizeof(Queue) * size)) == NULL){
		printf("rlist.c: Could not allocate lists\n");
		return 0;
	}
	
	for(i = 0; i < size; i++){
		if((list->lists[i] = queueCreate(i)) == NULL){
			printf("rlist.c: Could not allocate queue level %d\n", i);
			return 0;
		}
	}

	list->count = 0;
	list->levels = size;
	return 1;
}
int rlistAdd(QData data){
	if(list != NULL){
		if(list->lists != NULL){
			return enqueue(list->lists[data.priority], data);
		}
	}
	return 0;
}
int rlistPop(int priority, QData *pop){
	if(list != NULL){
		if(list->lists != NULL){
			return dequeue(list->lists[priority], pop);
		}
	}
	printf("rlist.c: No node to pop from level %d\n", priority);
	return 0;
}

int rlistRemove(int pid, QData *pop){
	int i;
	if(list != NULL){
		if(list->lists != NULL){
			for(i = 0; i < list->levels; i++){
				if(queueRemove(list->lists[i], pid, pop))
					return 1;
			}
		}
	}
	printf("rlist.c: Process %d not found\n", pid);
	return 0;
}
void rlistDestroy(){
	int i;
	if(list != NULL){
		if(list->lists != NULL){
			for(i = 0; i < list->levels; i++){
				queueDestroy(list->lists[i]);
			}
			free(list->lists);
		}
	free(list);
	list = NULL;
	}
}

void rlistPrint(){
	int i;
	if(list != NULL){
		if(list->lists != NULL){
			for(i = 0; i < list->levels; i++){
				queuePrint(list->lists[i]);
			}
		}
		else printf("rlist.c: Empty ready list\n");
	}
	else printf("rlist.c: No ready list\n");
}
