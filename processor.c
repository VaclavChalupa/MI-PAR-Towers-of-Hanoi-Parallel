/*
 * processor.c
 *
 */

#define STACK_ITEM_ATTRS  4
#define CHECK_MSG_AMOUNT  20

#define WHITE			  1
#define BLACK			  0

#define MSG_WORK_REQUEST 1000
#define MSG_WORK_SENT    1001
#define MSG_WORK_NOWORK  1002
#define MSG_TOKEN        1003
#define MSG_INIT		 1004
#define MSG_FINISH       1005
#define MSG_RESULT		 1006

#include "state_printer.h";
#include "tower.h"
#include "stack_item.h"
#include "stack.h"
#include "process_item.h"
#include "analyser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mpi.h"

int towersCount;
Tower *towers;
int discsCount;
int min; // by theory, solution cannot be better than this
int max; // solution definitely won't be more expensive than this
int currentSteps;
int minSteps; // best solution
int destTower;
int process_id;
int processors;
int waiting;
int idle;
Stack* stack;
typedef struct SolutionQueue {
	ProcessItem *head;
} SolutionQueue;
SolutionQueue* sq;

static void describeMove(int* prevState, int* currentState, int* disc,
		int* sourceTower, int* destTower);
static int compareStates(int* prevState, int* currentState);
static void inspectStack(Stack * stack, struct SolutionQueue* sq);
static int* serializeState(Tower* _towers);
static Tower* deserializeState(int* data);
static int* serializeStack(Stack* stack);
static void deserializeStack(int* data, int size);
static int loopDetected(Stack* stack);
static void processStepWithStack(struct SolutionQueue* sq);
void freeInspectStack(struct SolutionQueue* sq);
void askForWork(int processor);
void finalize();

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
	stackData = (int*) malloc(
			stack->num * (discsCount + STACK_ITEM_ATTRS) * sizeof(int));
	StackItem* item;
	item = stack->top;
	int offset;
	offset = 0;
	while (item != NULL) {
		int i;
		for (i = 0; i < discsCount + STACK_ITEM_ATTRS; i++) {
			if (i < discsCount) {
				stackData[offset + i] = item->data[i];
			} else if (i == discsCount) {
				stackData[offset + i] = item->step;
			} else if (i == discsCount + 1) {
				stackData[offset + i] = item->i;
			} else if (i == discsCount + 2) {
				stackData[offset + i] = item->j;
			} else if (i == discsCount + 3) {
				stackData[offset + i] = item->movedDisc;
			}
		}
		offset += (discsCount + STACK_ITEM_ATTRS);
		item = item->next;
	}
	return stackData;
}

void deserializeStack(int* stackData, int size) {
	int offset, bulk;
	bulk = discsCount + STACK_ITEM_ATTRS;
	for (offset = 0; offset < size; offset += bulk) {
		int* data;
		data = (int*) malloc(discsCount * sizeof(int));
		int step, i, j, movedDisc;
		int disc;
		for (disc = 0; disc < discsCount; disc++) {
			data[disc] = stackData[offset + disc];
		}
		step = stackData[offset + discsCount];
		i = stackData[offset + discsCount + 1];
		j = stackData[offset + discsCount + 2];
		movedDisc = stackData[offset + discsCount + 3];
		push(data, step, i, j, movedDisc);
	}
}

int* serializeSolution() {
	int* data;
	int i;
	i = 0;
	ProcessItem* item;
	item = sq->head;
	while (item != NULL) {
		data[i++] = item->disc;
		data[i++] = item->sourceTower;
		data[i++] = item->destTower;
		item = item->next;
	}
	return data;
}

SolutionQueue* deserializeSolution(int* data, int size) {
	SolutionQueue* sq;
	sq = malloc(sizeof(SolutionQueue));
	int i;
	for (i = 0; i < size;) {
		ProcessItem* item;
		item = malloc(sizeof(ProcessItem));
		item->next = sq->head;
		item->disc = data[i++];
		item->sourceTower = data[i++];
		item->destTower = data[i++];
		sq->head = item;
	}
	return sq;
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

	int counter;
	counter = 0;

	if (isStackEmpty()) {
		askForWork(-1);
	} else {
		int step, iStart, jStart, prevMovedDisc, i, moved = 0;
		int* stack_data;
		Tower* _towers;

		stack_data = top(&step, &iStart, &jStart, &prevMovedDisc);

		_towers = deserializeState(stack_data);

		if (step > minSteps || loopDetected(stack)) {
			/* not a perspective branch solution */
			pop();
			freeTowers(_towers, &towersCount);
			return;
		}

		if (isDestTowerComplete(&_towers[destTower - 1], discsCount)) {

			printf("\n\n\n\n------------------------------------FOUND %i\n",
					step);

			if (step < minSteps) {
				max = step;
				minSteps = step;
				inspectStack(stack, sq);
			}

			pop();
			freeTowers(_towers, &towersCount);

			if (step <= min) {
				// the best solution found!! (there cannot be anything better than this)
				printf("\n\n\n\n--BEST SOLUTION FOUND at step %i\n", step);
				fflush(stdout);
				MPI_Send("F", 1, MPI_CHAR, MPI_ANY_SOURCE, MSG_FINISH,
						MPI_COMM_WORLD);
			} else {
				return;
			}
		}

		for (i = iStart; i < towersCount; i++) {
			int j;
			for (j = jStart; j < towersCount; j++) {
				int resultDisc;
				if (i == j) {
					// skip "moving" disc to the same tower
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
						if (prevMovedDisc != resultDisc) {
							push(serializeState(_towers), step + 1, 0, 0,
									resultDisc);
							moved++;
						}
					}
					break;
				}
				jStart = 0;
			}
			if (moved > 0) {
				// disc with size i has been moved -> let's continue now with that step...
				// ...however, if we then find out moving that disc doesn't lead to a solution,
				// we will get back in the stack and move another disc
				break;
			}
		}
		if (moved == 0) {
			// after trying all the possibilities, I couldn't move any disc at this step
			// -> go back one step and try another branch
			pop();
		}
		freeTowers(_towers, &towersCount);
	}
}

/** My stack is now empty... I just wonder if there is some more work to be done?
 *  @param processor if negative, randomly select one to ask
 */
void askForWork(int processor) {
	idle = 1;
	if (process_id == 0) {
		// if processor 0 doesn't have any more work, send finalization token
		printf("\nProcessor 0 issues finalization token");
		fflush(stdout);
		int color;
		color = BLACK;
		MPI_Send(&color, 1, MPI_INT, 1, MSG_TOKEN, MPI_COMM_WORLD);
		//sleep(1);
		if (minSteps <= max) {
			// print best solution
			ProcessItem* pi;
			pi = sq->head;
			printf("\n\nDONE, Steps: %i\n\n", minSteps);
			fflush(stdout);
			while (pi != NULL) {
				printProcessItem(pi);
				pi = pi->next;
			}
		} else {
			printf("\nERROR: No solution found\n");
			fflush(stdout);
		}
		return;
	}
	if (processor < 0) {
		processor = random() % processors;
		if (processor == process_id) {
			// instead of sending to me, send to master
			processor = 0;
		}
	}
	printf("\nProcessor %i is asking processor %i for work", process_id,
			processor);
	fflush(stdout);
	MPI_Send("REQ", 3, MPI_CHAR, processor, MSG_WORK_REQUEST, MPI_COMM_WORLD);
	//sleep(1); // wait a little while
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

/** Erase solution queue. */
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

/** Process 0 first computes one level to have something to start from. After that, other processes may jump in. */
int process0(Tower *_towers, int _towersCount, int _discsCount, int _destTower) {
	towers = _towers;
	towersCount = _towersCount;
	discsCount = _discsCount;
	destTower = _destTower;

	sq = malloc(sizeof(SolutionQueue));
	sq->head = NULL;

	// calculate min and max (we know that the solution will be between those two)
	min = minMoves(towers, towersCount, discsCount, destTower);
	max = minSteps = maxMoves(discsCount, towersCount);
	max = 30;

	printf("\nmin: %i, max: %i , %i", min, max, minSteps);

	stack = initializeStack();
	/* initial state */
	push(serializeState(towers), 0, 0, 0, -1);

	processStepWithStack(sq);

	return minSteps;
}

/** Start computing and messaging. Accepts parameters that pass the information about processors. */
void run(int _process_id, int _processors) {
	printf("\nCalling run on processor %i", _process_id);
	fflush(stdout);
	sq = malloc(sizeof(SolutionQueue));
	process_id = _process_id;
	processors = _processors;
	// the solution queue should not be here
	sq->head = NULL;
	// end of
	if (process_id != 0) { // master (rank 0) processor was initialized when process0(...) was called
		stack = initializeStack();
		idle = 1;
	}
	int counter = 0;
	waiting = 1;
	while (waiting || !isStackEmpty()) {
		counter++;
		if ((counter % CHECK_MSG_AMOUNT) == 0) {
			int flag;
			MPI_Status status;
			MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag,
					&status);
			if (flag) {
				// a message arrived, we need to react
				// status contains tag (status.MPI_TAG), sender's process id (status.MPI_SOURCE)
				// and error number (status.MPI_ERROR)
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
					printf(
							"\nProcess %i received data: towersCount=%i, discsCount=%i",
							process_id, towersCount, discsCount);
					fflush(stdout);
					askForWork(0);
				}
					break;
				case MSG_WORK_REQUEST: { // another process requests some work
					char* text;
					MPI_Recv(&text, status._count, MPI_CHAR, status.MPI_SOURCE,
							status.MPI_TAG, MPI_COMM_WORLD, &status);
					printf("\nProcessor %i received request from %i",
							process_id, status.MPI_SOURCE);
					fflush(stdout);
					if (stackSize() > processors) {
						// ok, I have some work for you
						int* data;
						Stack* divided;
						printf("\nProcessor %i is going to divide its stack.",
								process_id);
						fflush(stdout);
						divided = divideStack();
						printf(
								"\nProcessor %i has divided its stack and is now going to send data to %i",
								process_id, status.MPI_SOURCE);
						fflush(stdout);
						data = serializeStack(divided);
						printf("\nSending data from %i to %i with length %i",
								process_id, status.MPI_SOURCE,
								(discsCount + STACK_ITEM_ATTRS));
						fflush(stdout);
						MPI_Send(data, discsCount + STACK_ITEM_ATTRS, MPI_INT,
								status.MPI_SOURCE, MSG_WORK_SENT,
								MPI_COMM_WORLD);
						printf("\nProcessor %i replies to %i by sending data.",
								process_id, status.MPI_SOURCE);
						fflush(stdout);
					} else {
						// I have no work to send you
						MPI_Send("N", 1, MPI_CHAR, status.MPI_SOURCE,
								MSG_WORK_NOWORK, MPI_COMM_WORLD);
						printf(
								"\nProcessor %i replies to %i saying 'I don't have anything for you'.",
								process_id, status.MPI_SOURCE);
						fflush(stdout);
					}
				}
					break;
				case MSG_WORK_SENT: { // new work has arrived!
					int data[status._count / sizeof(int)];
					// receive the message
					printf("\nProcessor %i received work from %i", process_id,
							status.MPI_SOURCE);
					fflush(stdout);
					MPI_Recv(&data, status._count / sizeof(int), MPI_INT,
							status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD,
							&status);
					// deserialize
					deserializeStack(data, status._count / sizeof(int));
					// recreate the latest tower
					int step, i, j, prevMovedDisc;
					towers = deserializeState(
							top(&step, &i, &j, &prevMovedDisc));
					// start processing
					idle = 0;
					// will start automatically, as the processStepWithStack method is called at the end of this loop
				}
					break;
				case MSG_WORK_NOWORK: {
					char* text;
					MPI_Recv(&text, status._count, MPI_CHAR, status.MPI_SOURCE,
							status.MPI_TAG, MPI_COMM_WORLD, &status);
					idle = 1;
					// process I requested to give me work has nothing either
					printf(
							"\nProcessor %i received 'I have no work for you' from %i",
							process_id, status.MPI_SOURCE);
					fflush(stdout);
					// let's try another process
					sleep(1);
					askForWork(-1);
					// (or switch to passive state and wait for token)
				}
					break;
				case MSG_TOKEN: {
					// finishing token
					// white flag means someone is still working
					int color;
					MPI_Recv(&color, status._count, MPI_INT, status.MPI_SOURCE,
							status.MPI_TAG, MPI_COMM_WORLD, &status);
					printf("\nColor received by %i is %i", process_id, color);
					fflush(stdout);
					if (process_id == 0) {
						if (color == BLACK) {
							// all processes have finished -> finalize
							int i;
							for (i = 1; i < processors; i++) {
								// send finalization message to all processors
								printf("\nSending finalization message to %i", i);
								fflush(stdout);
								MPI_Send("END", 3, MPI_CHAR, i, MSG_FINISH,
										MPI_COMM_WORLD);
							}
							// continue the loop to collect results from other processors
							break;
						}
					}
					if (!isStackEmpty()) {
						color = WHITE;
					}
					// pass the token to the next processor (or wrap to 0)
					printf("\nSending '%i' token from processor %i to %i", color, process_id, (process_id+1)%processors);
					fflush(stdout);
					MPI_Send(&color, 1, MPI_INT, (process_id + 1) % processors,
							MSG_TOKEN, MPI_COMM_WORLD);
				}
					break;
				case MSG_FINISH: { // this is the end...
					char* text;
					printf("\nProcessor %i receives finalization message.", process_id);
					fflush(stdout);
					MPI_Recv(&text, 3, MPI_CHAR, status.MPI_SOURCE,
							status.MPI_TAG, MPI_COMM_WORLD, &status);
					// if I have a solution, I send it to process 0
					int* solution;
					//solution = serializeSolution();
					if (solution != NULL) {
						MPI_Send(solution, minSteps * 3, MPI_INT, 0, MSG_RESULT,
								MPI_COMM_WORLD);
					}
					//nasledne ukoncim svoji cinnost
					//jestlize se meri cas, nezapomen zavolat koncovou barieru MPI_Barrier (MPI_COMM_WORLD)
					finalize();
				}
					break;
				case MSG_RESULT: { // collect solutions from other processors
					int* data;
					MPI_Recv(&data, status._count, MPI_INT, status.MPI_SOURCE,
							status.MPI_TAG, MPI_COMM_WORLD, &status);
					SolutionQueue* sq;
					sq = deserializeSolution(data, status._count);
					if (1) { // all solutions received
						finalize();
					}
				}
					break;
				default:
					perror("unknown message type");
					break;
				}
			}
		}
		if (!idle) {
			processStepWithStack(sq);
		}
	}
}

void finalize() {
	printf("\nFinalizing process %i\n", process_id);
	fflush(stdout);
	int i = 0;
	for (i = 0; i < towersCount; i++) {
		freeDiscs(&towers[i]);
	}
	free(towers);
	freeStack();
	MPI_Finalize();
	exit(0);
}
