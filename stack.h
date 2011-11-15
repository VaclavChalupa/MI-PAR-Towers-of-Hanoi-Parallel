/*
 * stack.h
 *
 */

#ifndef STACK_H_
#define STACK_H_

#include "stack_item.h"

typedef struct {
	/** reference to the top item */
	StackItem *top;
	/** size of the stack */
	int num;
} Stack;

Stack * initializeStack();

void push(int* data, int step, int i, int j);

int* top(int* step, int* i, int* j);

void pop();

void setState(int _i, int _j);

int isStackEmpty();

void freeStack();

void setReturning();

Stack* divideStack();

int stackSize();

#endif /* STACK_H_ */
