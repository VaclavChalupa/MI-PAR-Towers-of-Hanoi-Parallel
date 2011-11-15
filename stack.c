/*
 * stack.c
 *
 */

#include "stack.h"
#include <stdio.h>
#include <stdlib.h>

Stack * stack;
int x = 0;
static void pushItem(StackItem* stackItem);

Stack * initializeStack() {
	stack = malloc(sizeof(* stack));
	stack->top = NULL;
	stack->num = 0;
	return stack;
}

void freeStack() {
	StackItem* stackItem = stack->top;
	while(stackItem != NULL) {
		StackItem* tmp = stackItem->next;
		free(stackItem->data);
		free(stackItem);
		stackItem = tmp;
	}
	free(stack);
}

void pushItem(StackItem* stackItem) {
	stackItem->next = stack->top;
	stack->top = stackItem;
	stack->num++;
}

void push(int* data, int step, int i, int j, int movedDisc) {
	/*printf("\nPUSH\n");*/
	StackItem* stackItem;
	stackItem = (StackItem*) malloc(sizeof(* stackItem));

	stackItem->data = data;
	stackItem->next = NULL;
	stackItem->step = step;
	stackItem->i = i;
	stackItem->j = j;
	stackItem->movedDisc = movedDisc;
	pushItem(stackItem);
}

void setState(int _i, int _j) {
	if(!isStackEmpty()) {
		stack->top->i = _i;
		stack->top->j = _j;
	}
}

int* top(int* step, int* i, int* j, int* movedDisc) {
	if(isStackEmpty()) {
		perror("ERROR: stack is empty for top");
		return NULL;
	}
	*step = stack->top->step;
	*i = stack->top->i;
	*j = stack->top->j;
	*movedDisc = stack->top->movedDisc;
	return stack->top->data;
}

void pop() {
	/*printf("\nPOP\n");*/
	StackItem* tmp;
	if(isStackEmpty()) {
		perror("ERROR: stack is empty for pop");
		return;
	}

	stack->num--;
	tmp = stack->top;
	stack->top = stack->top->next;
	free(tmp->data);
	free(tmp);
}

int isStackEmpty() {
	if(stack->top != NULL) {
		return 0;
	}
	return 1;
}

int stackSize() {
	return stack->num;
}

Stack* divideStack() {
	Stack* divided;
	divided = malloc(sizeof(* divided));
	divided->top = NULL;
	divided->num = 0;
	StackItem* item;
	StackItem* prev;
	item = stack->top;
	int i;
	while (item->next != NULL) {
		i++;
		if (i%2 == 0) {
			// take away from current stack
			StackItem* tmp;
			tmp = item->next;
			tmp->next = prev;
			// put on the divided stack
			divided->top = item;
		}
		prev = item;
		item = stack->top;
	}
	return divided;
}
