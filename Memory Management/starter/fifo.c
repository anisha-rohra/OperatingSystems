#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include "pagetable.h"


extern int memsize;

extern int debug;

extern struct frame *coremap;

// q keeps track of the first in frame number
int q;

/* Page to evict is chosen using the fifo algorithm.
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */
int fifo_evict() {
	// save the frame that will be returned (the current first-in)
	int frame = q;

	if (q == memsize - 1) {
		// if q is memsize, then go back to the start of coremap to reset first-in
		q = 0;
	} else {
		// increment q to the next frame number because that is now first-in
		q++;
	}

	return frame;
}

/* This function is called on each access to a page to update any information
 * needed by the fifo algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
void fifo_ref(pgtbl_entry_t *p) {

	// nothing needs to be updated upon reference
	return;
}

/* Initialize any data structures needed for this
 * replacement algorithm
 */
void fifo_init() {

	// the 'first in'for the first evict will be at coremap[0],
	// and so the frame to be evicted the first time should be 0
	q = 0;

}
