#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "ext2.h"

int dirs_num;

char type(unsigned short mode) {
  char type = '\0';
  if (mode == (EXT2_S_IFDIR >> 12)) {
    type = 'd';
  }
  else if (mode == (EXT2_S_IFREG >> 12)) {
    type = 'f';
  }
  else if (mode == (EXT2_S_IFLNK >> 12)) {
    type = 's';
  }
  return type;
}

char type2(unsigned char mode) {
  char type = '\0';
  if (mode == EXT2_FT_UNKNOWN) {
    type = 'u';
  }
  else if (mode == EXT2_FT_REG_FILE) {
    type = 'f';
  }
  else if (mode == EXT2_FT_DIR) {
    type = 'd';
  }
  else if (mode == EXT2_FT_SYMLINK) {
    type = 's';
  }
  return type;
}

char **split_path(char *path) {
  dirs_num = 0;
  char path_copy[strlen(path)];
  strcpy(path_copy, path);
  char *token;
  token = strtok(path_copy, "/");

  int i = 0;
  while (token != NULL) {
    token = strtok(NULL, "/");
    i++;
  }

  dirs_num = i;
  char **dirs = malloc(sizeof(char *) * (dirs_num));
  char *entry = strtok(path, "/");
  i = 1;
  dirs[0] = malloc(sizeof(char) * strlen(entry));
  dirs[0] = entry;

  while (entry != NULL) {
    entry = strtok(NULL, "/");

    if (entry != NULL) {
      dirs[i] = malloc(sizeof(char) * strlen(entry));
      dirs[i] = entry;
    }
    i++;
  }

  return dirs;
}