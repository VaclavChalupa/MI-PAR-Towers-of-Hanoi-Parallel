/*
 * tower.c
 *
 */

#include "tower.h"
#include "disc.h"
#include <stdlib.h>
#include <stdio.h>

static int processMove(Tower *source, Tower *dest);

static int verify(Tower *tower);

int insertDics(int size, Tower *tower) {
	Disc *disc = malloc(sizeof(* disc));
	disc->size = size;
	disc->next = NULL;

	if(tower->top != NULL) { /* NULL */
		disc->next = tower->top;
	}

	tower->top = disc;
	return 0;
}

int move(Tower *source, Tower *dest) {
	if(source->top != NULL && dest->top != NULL && dest->top->size < source->top->size) {
		return -2; /* dest has smaller disc*/
	}
	return processMove(source, dest);
}


int verify(Tower *tower) {
	Disc *disc;
	int prev = -1;
	disc = tower->top;

	while(disc != NULL) {
		if (prev > -1 && disc->size <= prev ) {
			printf("ERROR in tower: %i", tower->number);
			exit(1);
		}
		disc = disc->next;
	}
	return 0;
}

int undoMove(Tower *source, Tower *dest) {
	verify(source);
	verify(dest);
	return processMove(dest, source);
}

int isDestTowerComplete(Tower *tower, int discCount) {
	Disc *disc;
	int index = 1;
	disc = tower->top;
	while(disc != NULL) {
		/*printf("%i - %i", index, disc->size);*/
		if (disc->size != index++) {
			return 0;
		}
		disc = disc->next;
	}
	return index == discCount+1 ? 1 : 0;
}

int processMove(Tower *source, Tower *dest) {
	Disc* tmp;

	if(source->top == NULL) {
		return -1; /* source has no disc */
	}

	tmp = source->top;
	source->top = source->top->next;
	tmp->next = dest->top;
	dest->top = tmp;

	return dest->top->size;
}

void freeDiscs(Tower *tower) {
	Disc *disc;
	disc = tower->top;
	while(disc != NULL) {
		free(disc);
		disc = disc->next;
	}
	tower->top = NULL;
}

