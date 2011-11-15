/*
 * stack_item.h
 *
 */

#ifndef STACK_ITEM_H_
#define STACK_ITEM_H_

typedef struct StackItem {
		/** Data are stored in format data[disc.size-1] = tower, i.e. on which tower you should place a disc indexed in this array (starting from 0). */
		int* data;
		/** Depth of the solution (how many steps have been taken since the beginning of processing to this point. */
		int step;
		/** Row in the Cartesian table (the "from" in [from tower, to tower]) with the solution attempt at the current level. */
		int i;
		/** Column in the Cartesian table (the "to" in [from tower, to tower]) with the solution attempt at the current level. */
		int j;
		/** Reference to the next item in the stack. */
        struct StackItem *next;

        /** Prev move detection **/
        int movedDisc;

} StackItem;

#endif /* STACK_ITEM_H_ */
