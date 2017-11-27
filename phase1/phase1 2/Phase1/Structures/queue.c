#include "queue.h"
#include <stdio.h>
#include <stdlib.h>

Queue *queueCreate(int priority){
	Queue *queue;
	if((queue = malloc(sizeof(Queue))) == NULL){
		printf("queue.c: Could not allocate queue\n");
		return NULL;
	}
	queue->head = NULL;
	queue->tail = NULL;
	if((queue->head = malloc(sizeof(Queue))) == NULL){
		printf("queue.c: Could not allocate head for queue\n");
		return NULL;
	}
	
	queue->tail = queue->head;
	queue->head->next = NULL;
	queue->count = 0;
	queue->priority = priority;
	return queue;
}

int enqueue(Queue *queue, QData data){
	QNode *new;
	if((new = malloc(sizeof(QNode))) == NULL){
		printf("Queue.c: Could not allocate node for queue\n");
		return 0;
	}
	
	new->data.pid = data.pid;
	new->data.priority = data.priority;
	new->next = NULL;
	queue->tail->next = new;
	queue->tail = new;
	queue->count++;
	return 1;
}

int dequeue(Queue *queue, QData *ret){
	QNode *pop;
	if(queue == NULL)
		return 0;
	if(queue->head == NULL)
		return 0;
	if(queue->head->next == NULL)
		return 0;
	
	pop = queue->head->next;
	ret->pid = pop->data.pid;
	ret->priority = pop->data.priority;
	queue->head->next = pop->next;
	pop->next = NULL;
	free(pop);
	queue->count--;	

	return 1;;
}

int queueRemove(Queue *queue, int pid, QData *ret){
	QNode *cur, *next;
	if(queue == NULL)
		return 0;
	if(queue->head == NULL)
		return 0;
	if(queue->head->next == NULL)
		return 0;

	for(cur = queue->head; cur->next != NULL; cur = cur->next){
		if(cur->next->data.pid == pid){
			next = cur->next;
			cur->next = next->next;
			next->next = NULL;
			if(next->data.pid == queue->tail->data.pid)
				queue->tail = cur;

			ret->pid = next->data.pid;
			ret->priority = next->data.pid;
			free(next);
			queue--;
			return 1;
		}
	}
	return 0;
}
void queueDestroy(Queue *queue){
	QNode *cur = NULL, *prev = NULL;
	if(queue != NULL){
		if(queue->head != NULL){
			for(cur = queue->head; cur != NULL;){
				prev = cur;
				cur = cur->next;
				prev->next = NULL;
				free(prev);
			}
	}
	queue->head = NULL;
	queue->tail = NULL;
	free(queue);
	}
}

void queuePrint(Queue *queue){
	QNode *cur;
	if(queue == NULL)
		printf("No queue\n");
	else if(queue->head == NULL)
		printf("Queue level %d headless\n", queue->priority);
	else if(queue->head->next == NULL)
		printf("Queue level %d empty\n", queue->priority);
	else{
		printf("Queue level %d: ", queue->priority);
		for(cur = queue->head->next; cur != NULL; cur = cur->next){
			printf("|%d|", cur->data.pid);
			if(cur->next != NULL)
				printf("->");
		}
		printf("\n");
	}
}
