#pragma once

#define GAME_STATE_SEM 0
#define MAP_SEM 1
#define PLAYERS_SEM_OFFSET 2

void semUnlock(int semId, int n);
void semLock(int semId, int n);