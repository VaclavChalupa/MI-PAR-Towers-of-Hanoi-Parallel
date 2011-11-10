/*
 * main.c
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tower.h"
#include "state_printer.h"
#include "analyser.h"
#include "processor.h"
#include "mpi.h"

// fflush(stdout) or cout<<flush after each printout

int process_id, processors;

int main(int argc, char *argv[]) {
	Tower *towers;
	int discsCount, towersCount, destTower;

	/* initialize MPI */
	MPI_Init(&argc, &argv);

	/* find out process id */
	MPI_Comm_rank(MPI_COMM_WORLD, &process_id);

	/* find out number of processes */
	MPI_Comm_size(MPI_COMM_WORLD, &processors);

	if (process_id == 0) {
		// I'm the starting process
		// I have to read the data and let others take portions of it
		static const char filename[] = "enter.txt";
		static const char discDelimiter[] = ",";
		FILE *file = fopen(filename, "r");
		if (file != NULL) {
			char line[128]; /* max line size */

			int i;
			fgets(line, sizeof line, file);
			sscanf(line, "%i", &discsCount);
			fgets(line, sizeof line, file);
			sscanf(line, "%i", &towersCount);
			fgets(line, sizeof line, file);
			sscanf(line, "%i", &destTower);

			printf("Towers of Hanoi: %i towers, %i discs, %i dest tower\n",
					towersCount, discsCount, destTower);

			towers = (Tower*) malloc(towersCount * sizeof(*towers));

			for (i = 0; i < towersCount; i++) {
				char towerLine[128];
				Tower tower = { 0, NULL };
				char *disc;

				tower.number = i + 1;

				if (fgets(towerLine, sizeof towerLine, file) == NULL) {
					return 1; /* bad enter */
				}

				printf("Created new tower: %i\n", tower.number);

				disc = strtok(towerLine, discDelimiter);
				while (1) {
					int discSize = 0;

					if (disc == NULL) {
						break;
					}

					sscanf(disc, "%i", &discSize);

					if (discSize == 0) {
						break;
					}

					insertDics(discSize, &tower);
					printf("Inserted disc of size: %i to tower %i\n", discSize,
							tower.number);
					disc = strtok(NULL, discDelimiter);
				}
				towers[i] = tower;
			}
			fclose(file);

			// split work
			for (i = 1; i < processors; i++) {
				//MPI_Send(message, strlen(message)+1, MPI_CHAR, i, MSG_FINISH, MPI_COMM_WORLD);
			}
		} else {
			perror("enter.txt could not be opened");
			return 1;
		}

	} else {
		// other processes (process_id > 0)

	}

	printState(towers, towersCount);

	process(towers, towersCount, discsCount, destTower);

	int i;
	for (i = 0; i < towersCount; i++) {
		freeDiscs(&towers[i]);
	}
	free(towers);

	printf("\n***END***\n");

	return 0;

}

