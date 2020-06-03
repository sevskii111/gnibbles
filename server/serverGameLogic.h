#pragma once
#include <netinet/in.h>
#include "string.h"
#include "settings.h"
#include "types.h"

void resetState(int semId, struct GameState *gameState, struct PlayerState *playersState, struct Map *map);
int acceptPlayers(int sockfd, int semId, struct GameState *gameState, struct PlayerState *playersState, struct Map *map);
void setupGame(int semId, struct PlayerState *playersState);
void playGame(int semId, struct GameState *gameState, struct PlayerState *playersState, struct Map *map);