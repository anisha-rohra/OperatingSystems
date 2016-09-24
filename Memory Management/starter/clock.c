#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include "pagetable.h"


extern int memsize;

extern int debug;

extern struct frame *coremap;

// the current place where the clock is starting at
int clock_handle;

/* Page to evict is chosen using the clock algorithm.
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */

int clock_evict() {

	int evict_frame;
	pgtbl_entry_t *p;

	// should keep going through the clock until you find a non-referenced
	// page, and at that point break
	while (1) {
		// if you've come to the end of coremap, start back at the beginning
	  if (clock_handle == memsize) {
			clock_handle = 0;
		}

		p = coremap[clock_handle].pte;

		if (!(p->frame & PG_REF)) {
			 // if R = 0, return clock_handle by breaking out of while loop
			evict_frame = clock_handle;
			break;
		} else {
			// clear reference bit for page with R = 1
			p->frame &= ~PG_REF;
		}

		//move clock handle to next page
		clock_handle ++;

	}
 	return evict_frame;
}

/* This function is called on each access to a page to update any information
 * needed by the clock algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
void clock_ref(pgtbl_entry_t *p) {

	// do not need to update anything upon reference

	return;
}

/* Initialize any data structures needed for this replacement
 * algorithm.
 */
void clock_init() {

	clock_handle = 0;  	//initialize clock handle

}
