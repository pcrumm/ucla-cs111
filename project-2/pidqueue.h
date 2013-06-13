#ifndef PID_QUEUE
#define PID_QUEUE

#include <stdbool.h>
#include "linkedlist.h"

typedef struct pid_queue {
	list_node_t *head;
	list_node_t *tail;
} pid_queue_t;

pid_queue_t* pid_queue_init (void)
{
	pid_queue_t *q = kmalloc(sizeof(pid_queue_t), GFP_ATOMIC);
	q->head = NULL;
	q->tail = NULL;

	return q;
}

/* Returns 1 if the queue has elements, 0 if empty */
bool pid_queue_empty (pid_queue_t *q)
{
	if(q->head == NULL)
		return true;

	return false;
}

void pid_queue_push (pid_queue_t *q, pid_t p)
{
	if(q->head == NULL)
		q->head = q->tail = list_add_to_back(NULL, p);
	else
		q->tail = list_add_to_back(q->tail, p)->next;
}

/**
 * Pops an element from the queue.
 */
pid_t pid_queue_pop (pid_queue_t *q)
{
	list_node_t *elem;
	pid_t ret;

	if(q->head == NULL)
		return -1;

	elem = q->head;
	ret = elem->pid;

	if(q->head == q->tail)
		q->head = q->tail = NULL;
	else
		q->head = q->head->next;

	kfree(elem);
	return ret;
}

void pid_queue_remove_all (pid_queue_t *q)
{
	list_free_all(q->head);
	q->head = q->tail = NULL;
}

void pid_queue_add_elements_from_list (pid_queue_t *q, list_node_t *head)
{

	if(q == NULL || head == NULL)
		return;

	do
	{
		pid_queue_push(q, head->pid);
	} while ((head = head->next) != NULL);
}
#endif // PID_QUEUE
