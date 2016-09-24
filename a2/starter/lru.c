#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include "pagetable.h"


extern int memsize;

extern int debug;

// we have added a counter to the struct frame that keeps track of where
// that frame is in the lru queue
extern struct frame *coremap;

// the universal count, ensures that no two frames in coremap
// have the same lru count and therefore guarentees exact lru
int count;

/* Page to evict is chosen using the accurate LRU algorithm.
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */
int lru_evict() {

	// traverse the coremap in order to find the min element because that
	// is the least recently used frame, since it has the smallest counter
	int min = count;
	int min_frame;
	int i;
	for (i = 0; i < memsize; i++) {
		if (coremap[i].counter < min) {
			min = coremap[i].counter;
			min_frame = i;
		}
	}

	return min_frame;
}

/* This function is called on each access to a page to update any information
 * needed by the lru algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
void lru_ref(pgtbl_entry_t *p) {

	// every time the page is referenced, set its counter to the
	// current value of the global count, then increment the count
	coremap[p->frame >> PAGE_SHIFT].counter = count;
	count++;

	return;
}


/* Initialize any data structures needed for this
 * replacement algorithm
 */
void lru_init() {

	// count is 0 only at the beginning
	count = 0;

	// set every counter in coremap to 0
	int i;
	for (i = 0; i < memsize; i++) {
		coremap[i].counter = 0;
	}

}
