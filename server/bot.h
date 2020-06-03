#pragma once 

#include "settings.h"

void encodeMap(char *x, char *y, int encoded[MAP_SIZE][MAP_SIZE], int map[MAP_SIZE][MAP_SIZE], char ind);
char makeTurn(char x, char y, int encoded[MAP_SIZE][MAP_SIZE]);
void *botTask(void *targs);