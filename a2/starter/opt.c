#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include "pagetable.h"
#include "sim.h"
#include <string.h>

extern unsigned int memsize;

extern int debug;

// cormap[i].core_vaddr is the virtual address assoicated with that frame
extern struct frame *coremap;

extern char *tracefile;

// the current line in the tracefile
int offset;

char vaddress[100];
char letter;

char **array;
int total_lines;
int frame_evict;

// struct to keep track of each line number that a vaddress appears at
typedef struct s {

	int line_number;
	struct s *next;

} stack;

// struct for each unique vaddress that appears in the tracefile
typedef struct p {

	char vaddr[100];

	// the first line number that the vaddress appears at
	stack *lines;

	// the last line the vaddress appears at
	stack *last;

	// whether or not the coremap contains this vaddress
	int in_use;

	struct p *next;

} pageLL;

pageLL *head;

/* A helper function for opt_init in order to find the total number of lines,
 * initialize the array of strings, and assign each virtual address to the
 * right index in the array
 */
void read_file() {
	int i;
	total_lines = 0;

	// finds total number of lines in tracefile
	FILE *file = fopen(tracefile, "r");
	while (fscanf(file, "%c %s ", &letter, vaddress) != EOF) {
		total_lines++;
	}

	// malloc the right amount of space for the array of strings
	array = malloc(total_lines * sizeof(char *));
	for (i = 0; i < total_lines; i++) {
		array[i] = malloc(sizeof(char) * 100);
	}

	// assign the virtual address in the tracefile to array[current_line]
	int current_line = 0;
	fseek(file, 0, SEEK_SET);
	while (fscanf(file, "%c %s ", &letter, vaddress) != EOF) {
		strcpy(array[current_line], vaddress);
		current_line++;
	}

	fclose(file);
}

/* A helper function called only once the first time opt_evict is
 * called, iterates through the coremap and finds the vaddress assoicated
 * with every frame in the coremap and initializes it.
 */
void offset_only() {

	int prev;
	int line_num = 0;
	int all = 0;

	// iterates over every frame in the coremap or until you reach the
	// end of the tracefile
	while ((all < memsize) && (line_num < total_lines)) {
		int exist = 0;
		for (prev = 0; prev < all; prev++) {

			// if you find that the vaddress at this line already exists
			// in the coremap, break and increment line_num
			if (strcmp(coremap[prev].core_vaddr, array[line_num]) == 0) {
				exist = 1;
				break;
			}
		}
		// if you do not find the vaddress already in the coremap, then
		// it must be added to the coremap at the current line_num
		if (exist == 0) {
			strcpy(coremap[prev].core_vaddr, array[line_num]);
			all++;
		}
		line_num++;
	}

	// goes through the linked list to set all the vaddresses that are in
	// the coremap to in_use = 1
	pageLL *curr = head;
	while (curr != NULL) {
		for (all = 0; all < memsize; all++) {
			if (strcmp(coremap[all].core_vaddr, curr->vaddr) == 0) {
				curr->in_use = 1;
				break;
			}
		}
		curr = curr->next;
	}

}

/* A helper function called every time opt_evict is called that
 * increments the offset to the place in the tracefile that has caused the
 * current page fault leading to an evict request.
 */
void increment_offset() {
	int i;
	int x;
	for (i = offset; i < total_lines; i++) {
		int found = 0;
		for (x = 0; x < memsize; x++) {

			// if the current vaddress is already in the coremap, break
			if (strcmp(coremap[x].core_vaddr, array[offset]) == 0) {
				found = 1;
				break;
			}
		}

		// only break if you didn't find the vaddress in the coremap
		// because only then have you reached a page fault
		if (found == 0) {
			break;
		}
		offset++;
	}
}

/* A helper function called at the end of every call of opt_evict that
 * iterates over the coremap to find the frame number whose vaddress
 * matches the given string.
 */
void find_evict(char *vaddr) {

	int i;
	for (i = 0; i < memsize; i++) {
		if (strcmp(coremap[i].core_vaddr, vaddr) == 0) {
			frame_evict = i;
			strcpy(coremap[frame_evict].core_vaddr, array[offset]);
			break;
		}
	}

}

/* Page to evict is chosen using the optimal (aka MIN) algorithm.
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */
int opt_evict() {
	int i;
	frame_evict = 0;
	pageLL *curr;
	for (i = 0; i < memsize; i++) {
		coremap[i].counter = 0;
		coremap[i].found = 0;
	}

	if (offset == 0) {
		offset_only();
	}

	increment_offset();

	int max = -1;
  curr = head;
	pageLL *final;
	while (curr != NULL) {
		// only want to do anything if the page is in the coremap
		if (curr->in_use == 1) {
			stack *curr_line = curr->lines;
			// finds the maximum line_number between all of the coremap frames
			while (curr_line != NULL) {
				if (curr_line->line_number > offset) {
					if (curr_line->line_number > max) {
						max = curr_line->line_number;
						final = curr;
					}
					break;
				} else {
					// reaching hear means that the curr->lines->line_number is
					// less than the offset, meaning it should be popped off
					curr->lines = curr_line->next;
					curr_line = curr_line->next;
				}
			}
			// if this statement is satisfied, it means curr->vaddr never
			// occurs again in the tracefile and thus should be chosen as the
			// frame to be evicted
			if (curr_line == NULL) {
				final = curr;
				break;
			}
		}
		curr = curr->next;
	}

	// reset the in_use variable for every node to be accurate to the new
	// state of the linked list after the eviction
	final->in_use = 0;
	curr = head;
	while (curr != NULL) {
		if (strcmp(curr->vaddr, array[offset]) == 0) {
			curr->in_use = 1;
			break;
		}
		curr = curr->next;
	}

	find_evict(final->vaddr);

	return frame_evict;
}

/* This function is called on each access to a page to update any information
 * needed by the opt algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
void opt_ref(pgtbl_entry_t *p) {

	// do not need to do anything on reference

	return;
}

/* Initializes any data structures needed for this
 * replacement algorithm.
 */
void opt_init() {

	int lines;
	offset = 0;
	lines = 0;

	read_file();

	// initialize the head of the linked list of pages
	head = malloc(sizeof(pageLL));
	head->vaddr[0] = '\0';

	FILE *file = fopen(tracefile, "r");
	while (fscanf(file, "%c %s ", &letter, vaddress) != EOF) {

		// if the head of the file hasn't been assigned a vaddress yet
		if (strlen(head->vaddr) == 0) {

			// initialization process
			strcpy(head->vaddr, vaddress);
			head->lines = malloc(sizeof(stack));
			head->lines->line_number = lines;
			head->next = NULL;
			head->lines->next = NULL;
			head->last = head->lines;
			head->in_use = 0;
		}

		else {

			pageLL *curr = head;
			while (curr->next != NULL) {

				// if the current vaddress equals the vaddress from the file,
				// add the current line number to the stack at the current node
				if (strcmp(curr->vaddr, vaddress) == 0) {

					// need to add current line to the linked list of lines
					// for the current node using the last pointer
					stack *curr_stack = curr->last;
					curr_stack->next = malloc(sizeof(stack));
					curr_stack->next->next = NULL;
					curr_stack->next->line_number = lines;
					curr->last = curr_stack->next;
					break;
				}
				curr = curr->next;
			}

			// if reached the end of the linked list
			if (curr->next == NULL) {

				// if the last node has the same vaddress
				if (strcmp(curr->vaddr, vaddress) == 0) {

					// need to add current line to the linked list of lines
					// for the current node using the last pointer
					stack *curr_stack = curr->last;
					curr_stack->next = malloc(sizeof(stack));
					curr_stack->next->line_number = lines;
					curr_stack->next->next = NULL;
					curr->last = curr_stack->next;
				}

				// otherwise, initialize a new node
				else {

					// initialization process
					curr->next = malloc(sizeof(pageLL));
					strcpy(curr->next->vaddr, vaddress);
					curr->next->lines = malloc(sizeof(stack));
					curr->next->lines->line_number = lines;
					curr->next->next = NULL;
					curr->next->lines->next = NULL;
					curr->next->last = curr->next->lines;
					curr->next->in_use = 0;
				}
			}
		}
		// increment the current line in the array
		lines++;
	}

	fclose(file);

}
