/*
 * processor.c
 *
 */

#define CHECK_MSG_AMOUNT  100

#define MSG_WORK_REQUEST 1000
#define MSG_WORK_SENT    1001
#define MSG_WORK_NOWORK  1002
#define MSG_TOKEN        1003
#define MSG_INIT		 1004
#define MSG_FINISH       1005

#include "state_printer.h";
#include "tower.h"
#include "stack_item.h"
#include "stack.h"
#include "process_item.h"
#include "analyser.h"
#include <stdio.h>
#include <stdlib.h>
#include "mpi.h"

int towersCount;
Tower *towers;
int discsCount;
int min;
int max;
int currentSteps;
int minSteps;
int destTower;
int process_id;
int processors;
int waiting;
struct SolutionQueue {
	ProcessItem *head;
};

static void describeMove(int* prevState, int* currentState, int* disc,
		int* sourceTower, int* destTower);
static int compareStates(int* prevState, int* currentState);
static void inspectStack(Stack * stack, struct SolutionQueue* sq);
static int* serializeState(Tower* _towers);
static Tower* deserializeState(int* data);
static int* serializeStack(Stack* stack);
static void deserializeStack(int* data);
static int loopDetected(Stack* stack);
static void processStepWithStack(struct SolutionQueue* sq);
void freeInspectStack(struct SolutionQueue* sq);
void askForWork(int processor);

int* serializeState(Tower* _towers) {
	int * stack_item_data, i;
	stack_item_data = (int*) malloc(discsCount * sizeof(int));

	for (i = 0; i < discsCount; i++) {
		stack_item_data[i] = -1;
	}

	for (i = 0; i < towersCount; i++) {
		Disc* disc;
		disc = _towers[i].top;
		while (disc != NULL) {
			stack_item_data[disc->size - 1] = i; /* disc size -> tower indexed form 0*/
			disc = disc->next;
		}
	}

	for (i = 0; i < discsCount; i++) {
		if (stack_item_data[i] == -1) {
			perror("ERROR: stack_item_data defect by serialization");
			return NULL;
		}
	}

	return stack_item_data;
}

/**
 * Restore towers and discs from the data from stack.
 */
Tower* deserializeState(int* data) {
	int i;
	Tower* _towers;

	_towers = (Tower*) malloc(towersCount * sizeof(Tower));

	for (i = 0; i < towersCount; i++) {
		_towers[i].number = i + 1;
		_towers[i].top = NULL;
	}

	for (i = discsCount - 1; i >= 0; i--) {
		insertDics(i + 1, &_towers[data[i]]);
	}

	return _towers;
}

int* serializeStack(Stack* stack) {
	int* stackData;
	StackItem* item;
	item = stack->top;
	int offset;
	offset = 0;
	while (item != NULL) {
		int i;
		for (i = 0; i < discsCount + 3; i++) {
			if (i < discsCount) {
				stackData[offset + i] = item->data[i];
			} else if (i == discsCount) {
				stackData[offset + i] = item->step;
			} else if (i == discsCount + 1) {
				stackData[offset + i] = item->i;
			} else if (i == discsCount + 2) {
				stackData[offset + i] = item->j;
			}
		}
		offset += (discsCount + 3);
		item = item->next;
	}
	return stackData;
}

void deserializeStack(int* stackData) {
	int offset, bulk;
	bulk = discsCount + 4;
	for (offset = 0; offset < sizeof(stackData); offset += bulk) {
		int* data;
		int step, i, j, movedDisc;
		int disc;
		for (disc = 0; disc < discsCount; disc++) {
			data[disc] = stackData[offset + disc];
		}
		step = stackData[offset + discsCount];
		i = stackData[offset + discsCount + 1];
		j = stackData[offset + discsCount + 2];
		movedDisc = j = stackData[offset + discsCount + 3];
		push(data, step, i, j, movedDisc);
	}
}

int loopDetected(Stack* stack) {
	StackItem * current, *tmp;
	int loop;
	current = stack->top;

	if (stack == NULL) {
		return 0;
	}

	for (tmp = current->next, loop = 0; tmp && loop < 20000;
			tmp = tmp->next, loop++) {
		if (compareStates(current->data, tmp->data)) {
			return 1;
		}
	}
	return 0;
}

void processStepWithStack(struct SolutionQueue* sq) {
	Stack * stack;
	int counter;
	stack = initializeStack();
	counter = 0;

	/* initial state */
	push(serializeState(towers), 0, 0, 0, -1);

	while (!isStackEmpty()) {
		int step, iStart, jStart, prevMovedDisc, i, moved = 0;
		int* stack_data;
		Tower* _towers;

		stack_data = top(&step, &iStart, &jStart, &prevMovedDisc);

		_towers = deserializeState(stack_data);

		if (step > max || loopDetected(stack)) {
			/* not a perspective branch solution */
			pop();
			freeTowers(_towers, &towersCount);
			continue;
		}

		if (isDestTowerComplete(&_towers[destTower - 1], discsCount)) {

			printf("\n\n\n\n------------------------------------FOUND %i\n", step);

			if (step < minSteps) {
				max = step;
				minSteps = step;
				inspectStack(stack, sq);
			}

			pop();
			freeTowers(_towers, &towersCount);

			if (step <= min) {
				break;
			} else {
				continue;
			}
		}

		for (i = iStart; i < towersCount; i++) {
			int j;
			for (j = jStart; j < towersCount; j++) {
				int resultDisc;
				if (i == j) {
					continue;
				}

				resultDisc = move(&_towers[i], &_towers[j]);

				if (resultDisc > 0) {
					if (j + 1 >= towersCount) {
						setState(i + 1, 0);
					} else {
						setState(i, j + 1);
					}
					if (moved == 0) {
						if(prevMovedDisc != resultDisc) {
							push(serializeState(_towers), step+1, 0, 0, resultDisc);
							moved++;
						}
					}
					break;
				}
				jStart = 0;
			}
			if (moved > 0) {
				break;
			}
		}
		if (moved == 0) {
			pop();
		}
		freeTowers(_towers, &towersCount);
	}
	freeStack();
	askForWork(-1);
}

/** My stack is now empty... I just wonder if there is some more work to be done?
 *  @param processor if negative, randomly select one to ask
 */
void askForWork(int processor) {
	if (process_id == 0) {
		// processor 0 shouldn't ask anyone for more work...
	//	return;
	}
 	if (processor < 0) {
		processor = random() % processors;
		if (processor == process_id) {
			// instead of sending to me, send to master
			processor = 0;
		}
	}
	printf("\nProcessor %i is asking processor %i for work", process_id, processor);
	fflush(stdout);
	MPI_Send("REQ", sizeof("REQ"), MPI_CHAR, processor, MSG_WORK_REQUEST,
			MPI_COMM_WORLD);
}

void describeMove(int* prevState, int* currentState, int* disc,
		int* sourceTower, int* destTower) {
	int i;
	for (i = 0; i < discsCount; i++) {
		if (prevState[i] != currentState[i]) {
			*disc = i + 1;
			*sourceTower = prevState[i] + 1;
			*destTower = currentState[i] + 1;
			return;
		}
	}
	*disc = *sourceTower = *destTower = -1;
}

/** Check if the two states are same. Returns true if they are same, false if they differ in at least one item. */
int compareStates(int* prevState, int* currentState) {
	int i;
	for (i = 0; i < discsCount; i++) {
		if (prevState[i] != currentState[i]) {
			return 0;
		}
	}
	return 1;
}

void inspectStack(Stack * stack, struct SolutionQueue* sq) {
	int* currentState;
	StackItem * tmp;
	ProcessItem * n;
	n = NULL;

	freeInspectStack(sq);

	tmp = stack->top;
	currentState = stack->top->data;

	for (tmp = tmp->next; tmp; tmp = tmp->next) {
		n = (ProcessItem*) malloc(sizeof(*n));
		describeMove(tmp->data, currentState, &n->disc, &n->sourceTower,
				&n->destTower);
		currentState = tmp->data;
		n->next = sq->head;
		sq->head = n;
	}
}

void freeInspectStack(struct SolutionQueue* sq) {
	ProcessItem * next;
	next = sq->head;
	while (next != NULL) {
		ProcessItem * tmp;
		tmp = next->next;
		free(next);
		next = tmp;
	}
	sq->head = NULL;
}

int process(Tower *_towers) {
	struct SolutionQueue sq;

	printf("\nPROCESS:\n");
	towers = _towers;

	sq.head = NULL;

	min = minMoves(towers, towersCount, discsCount, destTower);
	max = minSteps = maxMoves(discsCount, towersCount);
	max = 30;

	printf("\nmin: %i, max: %i , %i", min, max, minSteps);

	processStepWithStack(&sq);

	if (minSteps <= max) {
		ProcessItem* pi;
		pi = sq.head;
		printf("\n\nDONE, Steps: %i\n\n", minSteps);
		while (pi != NULL) {
			printProcessItem(pi);
			pi = pi->next;
		}
	} else {
		printf("\nERROR: No solution found\n");
	}

	return minSteps;
}

/** Called by processor 0 to pass in the arguments for calculation. */
void run(int _process_id, int _processors, int _towersCount, int _discsCount, int _destTower) {
	towersCount = _towersCount;
	discsCount = _discsCount;
	destTower = _destTower;
	run(_process_id, _processors);
}

/** Start computing and messaging. Accepts parameters that pass the information about processors. */
void run(int _process_id, int _processors) {
	printf("\nCalling run on processor %i", _process_id);
	fflush(stdout);
	process_id = _process_id;
	processors = _processors;
	int counter = 0;
	int waiting = 1;
	while (waiting || !isStackEmpty()) {
		counter++;
		if ((counter % CHECK_MSG_AMOUNT) == 0) {
			int flag;
			MPI_Status status;
			MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag,
					&status);
			if (flag) {
				//prisla zprava, je treba ji obslouzit
				//v promenne status je tag (status.MPI_TAG), cislo odesilatele (status.MPI_SOURCE)
				//a pripadne cislo chyby (status.MPI_ERROR)
				switch (status.MPI_TAG) {
				case MSG_INIT: {
					printf("\nProcess %i starting...", process_id);
					fflush(stdout);
					int initData[3];
					MPI_Recv(&initData, status._count, MPI_INT,
							status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD,
							&status);
					towersCount = initData[0];
					discsCount = initData[1];
					destTower = initData[2];
					printf("\nProcess %i received data: towersCount=%i, discsCount=%i", process_id, towersCount, discsCount);
					fflush(stdout);
					askForWork(0);
				}
					break;
				case MSG_WORK_REQUEST: { // another process requests some work
					printf("\nProcessor %i received request from %i", process_id, status.MPI_SOURCE);
					fflush(stdout);
					if (stackSize() > processors) {
						// ok, I have some work for you
						int* data;
						Stack* divided;
						divided = divideStack();
						data = serializeStack(divided);
						MPI_Send(data, sizeof(data), MPI_CHAR,
								status.MPI_SOURCE, MSG_WORK_SENT,
								MPI_COMM_WORLD);
					} else {
						// I have no work to send you
						MPI_Send("N", sizeof("N"), MPI_CHAR, status.MPI_SOURCE,
								MSG_WORK_NOWORK, MPI_COMM_WORLD);
					}
				}
					break;
				case MSG_WORK_SENT: { // new work has arrived!
					int* data;
					// receive the message
					MPI_Recv(&data, status._count, MPI_INT, status.MPI_SOURCE,
							status.MPI_TAG, MPI_COMM_WORLD, &status);
					printf("\nProcessor %i received work from %i", process_id, status.MPI_SOURCE);
					fflush(stdout);
					// deserialize
					deserializeStack(data);
					// recreate the latest tower
					int step, i, j, prevMovedDisc;
					Tower* towers = deserializeState(top(&step, &i, &j, &prevMovedDisc));
					// start processing
					process(towers, towersCount, discsCount, destTower);
				}
					break;
				case MSG_WORK_NOWORK: {
					// process I requested to give me work has nothing either
					// let's try another process
					askForWork(-1);
					// (or switch to passive state and wait for token)
				}
					break;
				case MSG_TOKEN: {
					// finishing token
					char* color;
					MPI_Recv(&color, 1, MPI_CHAR, status.MPI_SOURCE,
							status.MPI_TAG, MPI_COMM_WORLD, &status);
					// - bily nebo cerny v zavislosti na stavu procesu
					if (!isStackEmpty()) {
						color = "W";
					}
					// pass the token to the next processor (or wrap to 0)
					MPI_Send(color, 1, MPI_CHAR, (process_id+1) % processors, MSG_TOKEN, MPI_COMM_WORLD);
				}
					break;
				case MSG_FINISH: { //konec vypoctu - proces 0 pomoci tokenu zjistil, ze jiz nikdo nema praci
								   //a rozeslal zpravu ukoncujici vypocet
								   //mam-li reseni, odeslu procesu 0
					char* solution;
					MPI_Send(solution, 100/*solution size*/, MPI_CHAR, 0,
							MSG_FINISH, MPI_COMM_WORLD);
					//nasledne ukoncim svoji cinnost
					//jestlize se meri cas, nezapomen zavolat koncovou barieru MPI_Barrier (MPI_COMM_WORLD)
					int i = 0;
					for (i = 0; i < towersCount; i++) {
						freeDiscs(&towers[i]);
					}
					free(towers);

					printf("\n***END***\n");

					MPI_Finalize();
					exit(0);
				}
					break;
				default:
					perror("unknown message type");
					break;
				}
			}
		}
		process(towers);
	}
}
