#include "bot.h"

#include <unistd.h>
#include "sem.h"
#include "types.h"
#include "defines.h"
#include "../shared.h"

const int WALL = -1;
const int BLANK = -2;
const int TARGET = -3;

void encodeMap(char *x, char *y, int encoded[MAP_SIZE][MAP_SIZE], int map[MAP_SIZE][MAP_SIZE], char ind)
{
  for (int i = 0; i < MAP_SIZE; i++)
  {
    for (int j = 0; j < MAP_SIZE; j++)
    {
      if (map[i][j] == 0)
      {
        encoded[i][j] = BLANK;
      }
      else if (map[i][j] == 1)
      {
        encoded[i][j] = WALL;
      }
      else if (map[i][j] < FOOD_OFFSET)
      {
        int snakeId = (map[i][j] - 2) / MAX_WORM_LEN;
        int segmentInd = (map[i][j] - 2) % MAX_WORM_LEN;
        if (snakeId == ind && segmentInd == 0)
        {
          (*x) = j;
          (*y) = i;
        }
        encoded[i][j] = WALL;
      }
      else
      {
        encoded[i][j] = TARGET;
      }
    }
  }
}

char makeTurn(char sx, char sy, int encoded[MAP_SIZE][MAP_SIZE])
{
  int tx = -1, ty = -1;
  int mx = -1, my = -1;
  int dx[4] = {1, 0, -1, 0};
  int dy[4] = {0, 1, 0, -1};
  int d, x, y, k;
  char stop;

  d = 0;
  encoded[sy][sx] = 0;
  char targetFound = 0;
  do
  {
    stop = 1;
    for (y = 0; y < MAP_SIZE && !targetFound; ++y)
      for (x = 0; x < MAP_SIZE && !targetFound; ++x)
        if (encoded[y][x] == d)
        {
          for (k = 0; k < 4 && !targetFound; ++k)
          {
            int iy = y + dy[k], ix = x + dx[k];
            if (iy >= 0 && iy < MAP_SIZE && ix >= 0 && ix < MAP_SIZE)
            {
              if (encoded[iy][ix] == BLANK)
              {
                stop = 0;
                encoded[iy][ix] = d + 1;
                mx = ix;
                my = iy;
              }
              else if (encoded[iy][ix] == TARGET)
              {
                targetFound = 1;
                tx = ix;
                ty = iy;
                encoded[iy][ix] = d + 1;
              }
            }
          }
        }
    d++;
  } while (!stop && !targetFound);

  if (tx == -1 || ty == -1)
  {
    tx = mx;
    ty = my;
  }

  int len = encoded[ty][tx];
  x = tx;
  y = ty;
  d = len;
  int px[MAP_SIZE * MAP_SIZE];
  int py[MAP_SIZE * MAP_SIZE];
  while (d > 0)
  {
    px[d] = x;
    py[d] = y;
    d--;
    for (k = 0; k < 4; ++k)
    {
      int iy = y + dy[k], ix = x + dx[k];
      if (iy >= 0 && iy < MAP_SIZE && ix >= 0 && ix < MAP_SIZE &&
          encoded[iy][ix] == d)
      {
        x = x + dx[k];
        y = y + dy[k];
        break;
      }
    }
  }

  if (px[1] > sx)
  {
    return 3;
  }
  else if (px[1] < sx)
  {
    return 1;
  }
  else if (py[1] < sy)
  {
    return 4;
  }
  else
  {
    return 2;
  }
}

void *botTask(void *targs)
{
  struct WormThreadArgs *args = targs;
  int semId = args->semId;
  char ind = args->ind;
  struct PlayerState *playersState = args->playersState;
  struct PlayerState *myState = playersState + ind;
  struct GameState *gameState = args->gameState;
  struct Map *map = args->map;
  semLock(semId, PLAYERS_SEM_OFFSET + ind);
  myState->connected = 1;
  myState->ready = 1;
  semUnlock(semId, PLAYERS_SEM_OFFSET + ind);
  semLock(semId, GAME_STATE_SEM);
  gameState->playersConnected++;
  gameState->playersReady++;
  semUnlock(semId, GAME_STATE_SEM);

  struct WaitingState waitingState;
  do
  {
    semLock(semId, GAME_STATE_SEM);
    waitingState.gamePhase = gameState->gamePhase;
    semUnlock(semId, GAME_STATE_SEM);
  } while (waitingState.gamePhase == WAITING_FOR_PLAYERS);

  struct InGameState inGameState;
  char isAlive = 1;
  do
  {
    semLock(semId, GAME_STATE_SEM);
    inGameState.gamePhase = gameState->gamePhase;
    semUnlock(semId, GAME_STATE_SEM);

    semLock(semId, ind + PLAYERS_SEM_OFFSET);
    if (!myState->direction && myState->ready)
    {
      semUnlock(semId, ind + PLAYERS_SEM_OFFSET);
      semLock(semId, GAME_STATE_SEM);
      if (BOT_SPEED_UP || gameState->activeWorm == ind)
      {
        semUnlock(semId, GAME_STATE_SEM);
        semLock(semId, MAP_SEM);
        int encodedMap[MAP_SIZE][MAP_SIZE];
        char x, y;
        encodeMap(&x, &y, encodedMap, map->field, ind);
        semUnlock(semId, MAP_SEM);
        semLock(semId, ind + PLAYERS_SEM_OFFSET);

        myState->direction = makeTurn(x, y, encodedMap);
        semUnlock(semId, ind + PLAYERS_SEM_OFFSET);
      }
      else
      {
        semUnlock(semId, GAME_STATE_SEM);
      }
    }
    else
    {
      isAlive = myState->ready;
      semUnlock(semId, ind + PLAYERS_SEM_OFFSET);
    }
    usleep(1000000 / TICKRATE);
  } while (inGameState.gamePhase == IN_PROGRESS && isAlive);

  semLock(semId, ind + PLAYERS_SEM_OFFSET);
  myState->connected = 0;
  semUnlock(semId, ind + PLAYERS_SEM_OFFSET);
}
