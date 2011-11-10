/*
 * analyser.c
 *
 */

#include "analyser.h"
#include "tower.h"
#include <math.h>
#include <stdio.h>

int minMoves(Tower towers[], int size, int maxDiscSize, int destTower) {
	int count = 0, i;
	for(i = 0; i < size; i++) {
		if(towers[i].number == destTower) {
			int needed = maxDiscSize;
			Disc *disc;
			disc = towers[i].top;
			while(disc != NULL) {
				if(disc->size != needed) {
					count += 2;
				}
				needed--;
				disc = disc->next;
			}
		} else {
			Disc *disc;
			disc = towers[i].top;
			while(disc != NULL) {
				count++;
				disc = disc->next;
			}
		}
	}
	return count;
}

int maxMoves(int discsCount, int towersCount) {
	return ceil(pow(2, (discsCount/(towersCount-2)))-1)*(2*towersCount-5);
}

