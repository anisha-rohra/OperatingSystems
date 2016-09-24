#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "ext2.h"

char type(unsigned short mode);
char type2(unsigned char mode);
char **split_path(char *path);

unsigned char *disk;
extern int dirs_num;

int main(int argc, char **argv) {
	int i, j;
	if ((argc > 4) || (argc < 3)) {
        fprintf(stderr, "Usage: ext2_ls <image file name> <path>\n");
        exit(1);
    }

	int flag = 0;
	int c;
	int index;
	char *path, *image;
	while ((c = getopt(argc, argv, "a")) != -1) {
		switch (c) {
			case 'a':
				flag = 1;
				break;
			default:
				flag = 0;
		}
	}

	if ((flag == 0) && (argc == 4)) {
		exit(1);
	}

	for (index = optind; index < argc; index++) {
		if (argv[index][0] == '/') {
			path = argv[index];
		} else {
			image = argv[index];
		}
	}


	char **dir_names;

	if (strlen(path) > 1) {
		dir_names = split_path(path);
	}

	int fd = open(image, O_RDWR);
    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
		perror("mmap");
		exit(1);
    }

	struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);

	struct ext2_group_desc *gd = (struct ext2_group_desc *)(disk + EXT2_BLOCK_SIZE * 2);

	struct ext2_inode *curr_inode = (struct ext2_inode *) (disk + (EXT2_BLOCK_SIZE *
        gd->bg_inode_table) + (sb->s_inode_size * (1)));

	struct ext2_dir_entry_2 *dir_entry;

	i = 0;
	int last = 0;
	if (dirs_num == 0) {
		last = 1;
	}
	while (i <= dirs_num) {

		int found = 0;

		for (j = 0; j < 12 && found == 0; j++) {
			if (curr_inode->i_block[j] != 0) {
				if (type(curr_inode->i_mode >> 12) == 'd') {
					int start = 0;
					while (start < EXT2_BLOCK_SIZE && found == 0) {
						dir_entry = (struct ext2_dir_entry_2 *) (disk + (EXT2_BLOCK_SIZE * curr_inode->i_block[j]) + start);
						if (last == 0) {
							if (strcmp(dir_entry->name, dir_names[i]) == 0) {
								found = 1;
								curr_inode = (struct ext2_inode *) (disk + (EXT2_BLOCK_SIZE *
		    						gd->bg_inode_table) + (sb->s_inode_size * (dir_entry->inode - 1)));
							}
						} else {

								if (flag != 0 || ((strcmp(".", dir_entry->name) != 0) && (strcmp("..", dir_entry->name) != 0))) {
									printf("%s\n", dir_entry->name);
								}

						}
						start += (int)(dir_entry->rec_len);
					}
				}
			}
		}

		if (found == 0 && last == 0) {
			printf("No such file or directory");
			return -1;
		}

		i++;
		if (i == dirs_num) {
			last = 1;
		}

	}

	return 0;

}
