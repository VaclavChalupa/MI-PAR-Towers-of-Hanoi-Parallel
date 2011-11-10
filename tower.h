/*
 * tower.h
 *
 */

#ifndef TOWER_H_
#define TOWER_H_


#include "disc.h"

typedef struct {
	int number;
	Disc *top;
} Tower;


int insertDics(int size, Tower *tower);

int move(Tower * source, Tower *dest);

int undoMove(Tower * source, Tower *dest);

int isDestTowerComplete(Tower *tower, int discCount);

void freeDiscs();

#endif /* TOWER_H_ */
