#ifndef QUEUE_H
#define QUEUE_H

typedef struct qdata {
	int pid;
	int priority;
} QData;

typedef struct qnode {
	QData data;
	struct qnode *next;
} QNode;

typedef struct queue {
	QNode *head;
	QNode *tail;
	int count;
	int priority;
} Queue;

Queue *queueCreate(int priority);
int enqueue(Queue *queue, QData data);
int dequeue(Queue *queue, QData *data);
int queueRemove(Queue *queue, int pid, QData *data);
void queueDestroy(Queue *head);
void queuePrint(Queue *head);

#endif
