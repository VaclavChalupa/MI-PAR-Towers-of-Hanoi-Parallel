/*
 * stack_item.c
 *
 */

#include "process_item.h"
#include <stdio.h>

void printProcessItem(ProcessItem* processItem) {
	printf("\nMOVE: disc(%i), source tower: %i, dest tower: %i\n", processItem->disc, processItem->sourceTower, processItem->destTower);
}

