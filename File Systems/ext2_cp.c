#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include "ext2.h"


char type(unsigned short mode);
char type2(unsigned char mode);
char **split_path(char *path);
int get_free_block(unsigned char *bitmap);
int get_free_inode(unsigned char * bitmap);

unsigned char *disk;
extern int dirs_num;

int main(int argc, char **argv) {
	int i, j;
	int block_num1;

	if (argc != 4) {
        fprintf(stderr, "Usage: ext2_ls <image file name> <path to local file> <absolute path on virtual disk>\n");
        exit(1);
	}

	char **dir_names;
	char *path = argv[3];
	char *image = argv[1];


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

	unsigned char *block_bitmap = disk + (EXT2_BLOCK_SIZE * (gd->bg_block_bitmap));
	unsigned char *inode_bitmap = disk + (EXT2_BLOCK_SIZE * (gd->bg_inode_bitmap));

	struct ext2_inode *curr_inode = (struct ext2_inode *) (disk + (EXT2_BLOCK_SIZE *
        gd->bg_inode_table) + (sb->s_inode_size * (1)));

	struct ext2_dir_entry_2 *dir_entry;

	// Get to the current directory
	i = 0;

	while (i < dirs_num) {

		int found = 0;

		for (j = 0; j < 12 && found == 0; j++) {
			if (curr_inode->i_block[j] != 0) {
				if (type(curr_inode->i_mode >> 12) == 'd') {
					int start = 0;
					while (start < EXT2_BLOCK_SIZE && found == 0) {
						dir_entry = (struct ext2_dir_entry_2 *) (disk + (EXT2_BLOCK_SIZE * curr_inode->i_block[j]) + start);
						if (strcmp(dir_entry->name, dir_names[i]) == 0) {
							found = 1;
							curr_inode = (struct ext2_inode *) (disk + (EXT2_BLOCK_SIZE *
	    						gd->bg_inode_table) + (sb->s_inode_size * (dir_entry->inode - 1)));
							block_num1 = curr_inode->i_block[j];
						}
						start += (int)(dir_entry->rec_len);
					}
				}
			}
		}
		if (found == 0) {
			//printf("No such file or directory\n");
			return -ENOENT;
		}
		i++;
	}

	// Find the first free inode in the bitmap

		struct stat fileStat;
		if(stat(argv[2], &fileStat) < 0) {
        return -ENOENT;
		}

		int free_inode = get_free_inode(inode_bitmap);
		gd->bg_free_inodes_count --;

		struct ext2_inode *new_inode = (struct ext2_inode *) (disk + (EXT2_BLOCK_SIZE *
        gd->bg_inode_table) + (sb->s_inode_size * free_inode));
  	new_inode->i_mode = EXT2_S_IFREG;
		new_inode->i_size = fileStat.st_size;
		new_inode->i_links_count = 1;

		int used_blocks_for_file;
		if (new_inode->i_size % 1024 == 0){
			used_blocks_for_file = new_inode->i_size / 1024;
		}
		else {
			used_blocks_for_file = new_inode->i_size / 1024 + 1;
		}


  	FILE *file = fopen(argv[2], "rb");
		unsigned char *buffer = malloc(sizeof(unsigned char) * 1024);
  	i = 0;
  	int block_num = 0, k = 0;
		int bytes_read;
		unsigned char *free_space;
		int single_indirect;

		//checking direct and single indirect

		if (used_blocks_for_file < 12) {
  		while (((bytes_read = fread(buffer, 1, 1024,  file)) != 0) && (block_num < used_blocks_for_file)) {
				// Find the first free block in the bitmap
				int free_block = get_free_block(block_bitmap);
				gd->bg_free_blocks_count --;

				free_space = (disk + EXT2_BLOCK_SIZE * (free_block + 1));
				*free_space = *buffer;

				new_inode->i_block[block_num] = free_block + 1;
				buffer = buffer + bytes_read;
  			block_num++;
  		}
			new_inode->i_blocks = used_blocks_for_file;
			fclose(file);
		}

		else {
			while ((fread(buffer, 1, 1024,  file) != 0) && (block_num < used_blocks_for_file)) {
				//direct blocks
				if (block_num < 12) {
					int free_block = get_free_block(block_bitmap);
					gd->bg_free_blocks_count --;
					free_space = (disk + EXT2_BLOCK_SIZE * (free_block + 1));
					*free_space = *buffer;
					new_inode->i_block[block_num] = (free_block + 1);
				}
				//pointer to single indirect block
				else if (block_num == 12) {
					single_indirect = get_free_block(block_bitmap);
					gd->bg_free_blocks_count --;
					new_inode->i_block[block_num] = (single_indirect + 1);
				}
				//pointer to direct blocks from single indirect block
				else if (block_num > 12) {
					int new_block = get_free_block(block_bitmap);
					gd->bg_free_blocks_count --;
					*(disk + (EXT2_BLOCK_SIZE * (single_indirect + 1)) + i) = new_block;
					free_space = (disk + (EXT2_BLOCK_SIZE * (single_indirect+1)) + i);
					*free_space = *buffer;
					new_inode->i_block[block_num + i] = (new_block + 1);
				}
				buffer = buffer + 1024;
				block_num++;
			}
			new_inode->i_blocks = used_blocks_for_file + 1;
			fclose(file);
		}
		new_inode->i_blocks = new_inode->i_blocks * 2;

		//check for existing free block
		char **file_name = split_path(argv[2]);
		int new_name_len = (int)strlen(file_name[dirs_num - 1]);

		int curr_size = 0;
		while (curr_size < EXT2_BLOCK_SIZE) {
			dir_entry = (struct ext2_dir_entry_2 *) (disk + (EXT2_BLOCK_SIZE * block_num1) + curr_size);
	 		curr_size += dir_entry->rec_len;
		}

		int last_rec_len;
    if (dir_entry->name_len % 4 == 0) {
        last_rec_len = 8 + dir_entry->name_len;
    } else {
        last_rec_len = 8 + (dir_entry->name_len + (4 - (dir_entry->name_len % 4)));
    }
    int rest_rec_len = dir_entry->rec_len - last_rec_len;

    // calculate the new file information
    int new_rec_len;
    if (new_name_len % 4 == 0) {
        new_rec_len = new_name_len + 8;
    } else {
        new_rec_len = 8 + (new_name_len + (4 - (new_name_len % 4)));
    }

		struct ext2_dir_entry_2 *dir_entry2;
		dir_entry2 = (struct ext2_dir_entry_2 *) (disk + (EXT2_BLOCK_SIZE * block_num1));
		if (rest_rec_len >= new_rec_len) {
			dir_entry->rec_len = last_rec_len;
			dir_entry2 = (void *) (dir_entry) + dir_entry->rec_len;
			dir_entry2->inode = free_inode + 1;
			dir_entry2->rec_len = rest_rec_len;
			dir_entry2->name_len = new_name_len;
			dir_entry2->file_type = EXT2_FT_REG_FILE;
			strcpy(dir_entry2->name, file_name[dirs_num - 1]);
    }
		else {
			int free_block = get_free_block(block_bitmap);
			curr_inode->i_blocks += 2;
			dir_entry = (struct ext2_dir_entry_2 *) (disk + (EXT2_BLOCK_SIZE * (free_block+1)));
			dir_entry->inode = free_inode + 1;
			dir_entry->file_type = EXT2_FT_REG_FILE;
			strcpy(dir_entry->name, file_name[dirs_num - 1]);
			dir_entry->name_len = new_name_len;
			dir_entry->rec_len = EXT2_BLOCK_SIZE;
			for (j = 0; j < 12; j++) {
				if (curr_inode->i_block[j] == 0) {
					curr_inode->i_block[j] = free_block + 1;
					break;
				}
			}
	}
	return 0;

}
