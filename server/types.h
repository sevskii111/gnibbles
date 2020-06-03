#pragma once

#include "settings.h"

struct GameState
{
  char gamePhase;
  char playersConnected;
  char playersReady;
  char activeWorm;
  char foodOnMap;
};

struct PlayerState
{
  char connected;
  char ready;
  int length;
  char direction;
  char prevDirection;
};

struct Map
{
  int field[MAP_SIZE][MAP_SIZE];
  int rowVersions[MAP_SIZE];
  int mapVersion;
};

struct WormThreadArgs
{
  int sockfd;
  int semId;
  char ind;
  struct PlayerState *playersState;
  struct GameState *gameState;
  struct Map *map;
};