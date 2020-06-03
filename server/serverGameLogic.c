#include "serverGameLogic.h"

#include "stdlib.h"
#include "unistd.h"
#include <pthread.h>
#include "math.h"
#include "sem.h"
#include "../shared.h"
#include "defines.h"
#include "settings.h"
#include "wormLogic.h"
#include "bot.h"

void loadMap(int map[MAP_SIZE][MAP_SIZE])
{
  for (int i = 0; i < MAP_SIZE; i++)
  {
    for (int j = 0; j < MAP_SIZE; j++)
    {
      if (i == 0 || j == 0 || i == MAP_SIZE - 1 || j == MAP_SIZE - 1)
      {
        map[i][j] = 1;
      }
      else
      {
        map[i][j] = 0;
      }
    }
  }

  int inRow = ceil(sqrt(MAX_PLAYERS));
  float offset;
  if (inRow > 1)
  {
    offset = 1.0 * (MAP_SIZE - 3) / (inRow - 1);
  }
  else
  {
    offset = 0;
  }
  for (int i = 0; i < MAX_PLAYERS; i++)
  {
    int y = round(offset * (i / inRow));
    int x = round(offset * (i % inRow));
    map[1 + y][1 + x] = 2 + MAX_WORM_LEN * i;
  }
}

void resetState(int semId, struct GameState *gameState, struct PlayerState *playersState, struct Map *map)
{
  for (int i = 0; i < MAX_PLAYERS; i++)
  {
    semLock(semId, i + PLAYERS_SEM_OFFSET);
  }
  semLock(semId, MAP_SEM);
  semLock(semId, GAME_STATE_SEM);

  gameState->gamePhase = WAITING_FOR_PLAYERS;
  gameState->activeWorm = 0;
  gameState->foodOnMap = 0;
  for (int i = 0; i < MAX_PLAYERS; i++)
  {
    playersState[i].direction = 0;
    playersState[i].prevDirection = 0;
    playersState[i].length = 0;
    playersState[i].ready = 0;
  }
  bzero(map->rowVersions, sizeof(*map->rowVersions) * MAX_PLAYERS);
  map->mapVersion = 0;
  loadMap(map->field);

  for (int i = 0; i < MAX_PLAYERS; i++)
  {
    semUnlock(semId, i + PLAYERS_SEM_OFFSET);
  }
  semUnlock(semId, MAP_SEM);
  semUnlock(semId, GAME_STATE_SEM);
}

int acceptPlayers(int sockfd, int semId, struct GameState *gameState, struct PlayerState *playersState, struct Map *map)
{
  struct sockaddr_in cliaddr;
  socklen_t clilen = sizeof(cliaddr);
  while (1)
  {
    int newsockfd = accept(sockfd, (struct sockaddr *)&cliaddr, &clilen);
    if (newsockfd == 0)
    {
      return -1;
    }
    else if (newsockfd > 0)
    {
      for (int i = 0; i < MAX_PLAYERS; i++)
      {
        if (!playersState[i].connected)
        {
          pthread_t newWormThread;
          struct WormThreadArgs *newWormThreadArgs = malloc(sizeof(struct WormThreadArgs));
          newWormThreadArgs->sockfd = newsockfd;
          newWormThreadArgs->playersState = playersState;
          newWormThreadArgs->gameState = gameState;
          newWormThreadArgs->semId = semId;
          newWormThreadArgs->ind = (char)i;
          newWormThreadArgs->map = map;

          pthread_create(&newWormThread, NULL, wormTask, (void *)newWormThreadArgs);
          break;
        }
      }
    }
    semLock(semId, GAME_STATE_SEM);
    gameState->playersConnected = 0;
    gameState->playersReady = 0;
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
      semLock(semId, PLAYERS_SEM_OFFSET + i);
      if (playersState[i].connected)
      {
        gameState->playersConnected++;
      }
      if (playersState[i].ready)
      {
        gameState->playersReady++;
      }
      semUnlock(semId, PLAYERS_SEM_OFFSET + i);
    }
    if (gameState->playersConnected > 0 && gameState->playersConnected == gameState->playersReady)
    {
      gameState->gamePhase = IN_PROGRESS;
      semUnlock(semId, GAME_STATE_SEM);
      return 0;
    }
    semUnlock(semId, GAME_STATE_SEM);

    usleep(1000000 / TICKRATE);
  }

  return -1;
}

void addBots(int count, int semId, struct GameState *gameState, struct PlayerState *playersState, struct Map *map)
{
  for (int i = 0; i < MAX_PLAYERS && count > 0; i++)
  {
    if (!playersState[i].connected)
    {
      pthread_t newWormThread;
      struct WormThreadArgs *newWormThreadArgs = malloc(sizeof(struct WormThreadArgs));
      newWormThreadArgs->playersState = playersState;
      newWormThreadArgs->gameState = gameState;
      newWormThreadArgs->semId = semId;
      newWormThreadArgs->ind = (char)i;
      newWormThreadArgs->map = map;

      pthread_create(&newWormThread, NULL, botTask, (void *)newWormThreadArgs);
      count--;
    }
  }
}

void setupGame(int semId, struct PlayerState *playersState)
{
  for (int i = 0; i < MAX_PLAYERS; i++)
  {
    semLock(semId, i + PLAYERS_SEM_OFFSET);
    if (playersState[i].ready)
    {
      playersState[i].length = START_SNAKE_LEN;
    }
    semUnlock(semId, i + PLAYERS_SEM_OFFSET);
  }
  for (int i = 0; i < MAX_PLAYERS; i++)
  {
    semLock(semId, PLAYERS_SEM_OFFSET + i);
    playersState[i].ready = 1;
    semUnlock(semId, PLAYERS_SEM_OFFSET + i);
  }
}

char randFood()
{
  int r = rand() % 512;
  int n = 9;
  while (r > 0)
  {
    n--;
    r /= 2;
  }
  return 1 + n;
}

char gameLoop(int semId, struct GameState *gameState, int map[MAP_SIZE][MAP_SIZE], struct PlayerState playersState[MAX_PLAYERS], int rowVersions[MAP_SIZE], int *mapVersion)
{
  for (int i = 0; i < MAX_PLAYERS; i++)
  {
    semLock(semId, i + PLAYERS_SEM_OFFSET);
  }
  semLock(semId, MAP_SEM);
  semLock(semId, GAME_STATE_SEM);

  if (playersState[gameState->activeWorm].direction || !playersState[gameState->activeWorm].ready)
  {
    for (int i = 1; i <= MAX_PLAYERS; i++)
    {
      int ind = (gameState->activeWorm + i) % MAX_PLAYERS;
      if (playersState[ind].ready)
      {
        gameState->activeWorm = ind;
        break;
      }
    }
  }

  char allPlayersMadeTurn = 1;
  char playersOnField = 0;
  for (int i = 0; i < MAX_PLAYERS; i++)
  {
    if (playersState[i].ready)
    {
      playersOnField++;
      if (!playersState[i].direction)
      {
        allPlayersMadeTurn = 0;
      }
    }
  }

  if (playersOnField == 0)
  {
    gameState->gamePhase = SCOREBOARD;
  }
  else
  {
    char toRemoveC = 0;
    char toRemove[MAX_PLAYERS];
    if (allPlayersMadeTurn)
    {
      char lenIncrease[MAX_PLAYERS];
      char madeMove[MAX_PLAYERS];
      bzero(madeMove, sizeof(madeMove));
      bzero(lenIncrease, sizeof(lenIncrease));
      for (int i = 0; i < MAP_SIZE; i++)
      {
        for (int j = 0; j < MAP_SIZE; j++)
        {
          if (map[i][j] > 1 && map[i][j] < FOOD_OFFSET)
          {
            int snakeId = (map[i][j] - 2) / MAX_WORM_LEN;
            if (!playersState[snakeId].ready)
              continue;
            int segmentInd = (map[i][j] - 2) % MAX_WORM_LEN;
            if (segmentInd != 0 || !madeMove[snakeId])
            {
              map[i][j]++;
            }
            if (segmentInd == 0 && !madeMove[snakeId])
            {
              madeMove[snakeId] = 1;
              int targetX, targetY;
              char direction = playersState[snakeId].direction;
              if (direction == 1)
              {
                targetX = i;
                targetY = j - 1;
              }
              else if (direction == 2)
              {
                targetX = i + 1;
                targetY = j;
              }
              else if (direction == 3)
              {
                targetX = i;
                targetY = j + 1;
              }
              else if (direction == 4)
              {
                targetX = i - 1;
                targetY = j;
              }

              if (targetX < 0 || targetX >= MAP_SIZE || targetY < 0 || targetY >= MAP_SIZE)
              {
                playersState[snakeId].ready = 0;
                toRemove[toRemoveC] = snakeId;
                toRemoveC++;
              }
              else if (map[targetX][targetY])
              {
                if (map[targetX][targetY] < FOOD_OFFSET)
                {
                  if (map[targetX][targetY] > 1)
                  {
                    int targetSnakeId = (map[targetX][targetY] - 2) / MAX_WORM_LEN;
                    int targetSegmentId = (map[targetX][targetY] - 2) % MAX_WORM_LEN;
                    if (targetSegmentId + 1 >= playersState[targetSnakeId].length && (direction == 3 || direction == 2))
                    {
                      continue;
                    }
                  }
                  playersState[snakeId].ready = 0;
                  toRemove[toRemoveC] = snakeId;
                  toRemoveC++;
                }
                else
                {
                  gameState->foodOnMap--;
                  lenIncrease[snakeId] = map[targetX][targetY] - FOOD_OFFSET;
                  map[targetX][targetY] = 2 + MAX_WORM_LEN * snakeId;
                  (*mapVersion)++;
                  rowVersions[targetX]++;
                }
              }
              else
              {
                map[targetX][targetY] = 2 + MAX_WORM_LEN * snakeId;
                (*mapVersion)++;
                rowVersions[targetX]++;
              }
            }
            else if (segmentInd + 1 >= playersState[snakeId].length)
            {
              map[i][j] = 0;
              (*mapVersion)++;
              rowVersions[i]++;
            }
          }
        }
      }

      for (int i = 0; i < MAX_PLAYERS; i++)
      {
        if (playersState[i].ready)
        {
          playersState[i].length += lenIncrease[i];
          playersState[i].prevDirection = playersState[i].direction;
          playersState[i].direction = 0;
        }
      }

      for (int i = 0; i < MAX_PLAYERS; i++)
      {
        if (!madeMove[i] && playersState[i].ready)
        {
          playersState[i].ready = 0;
          toRemove[toRemoveC] = i;
          toRemoveC++;
        }
      }
    }

    for (int i = 0; i < MAX_PLAYERS; i++)
    {
      if (!playersState[i].connected && playersState[i].ready)
      {
        playersState[i].ready = 0;
        toRemove[toRemoveC] = i;
        toRemoveC++;
      }
    }

    for (int k = 0; k < toRemoveC; k++)
    {
      for (int i = 0; i < MAP_SIZE; i++)
      {
        for (int j = 0; j < MAP_SIZE; j++)
        {
          if (map[i][j] > 1 && map[i][j] < FOOD_OFFSET)
          {
            int snakeId = (map[i][j] - 2) / MAX_WORM_LEN;
            if (snakeId == toRemove[k])
            {
              map[i][j] = 0;
              (*mapVersion)++;
              rowVersions[i]++;
            }
          }
        }
      }
    }

    int a = 0;
    while (FOOD_TARGET - gameState->foodOnMap > 0 && a < 10000)
    {
      int x = rand() % MAP_SIZE;
      int y = rand() % MAP_SIZE;
      if (!map[x][y])
      {
        map[x][y] = FOOD_OFFSET + randFood();
        (*mapVersion)++;
        rowVersions[x]++;
        gameState->foodOnMap++;
      }
      a++;
    }
    for (int i = 0; i < MAP_SIZE && FOOD_TARGET - gameState->foodOnMap > 0; i++)
    {
      for (int j = 0; j < MAP_SIZE && FOOD_TARGET - gameState->foodOnMap > 0; j++)
      {
        if (!map[i][j])
        {
          map[i][j] = FOOD_OFFSET + randFood();
          (*mapVersion)++;
          rowVersions[i]++;
          gameState->foodOnMap++;
        }
      }
    }
  }
  for (int i = 0; i < MAX_PLAYERS; i++)
  {
    semUnlock(semId, i + PLAYERS_SEM_OFFSET);
  }
  semUnlock(semId, MAP_SEM);
  semUnlock(semId, GAME_STATE_SEM);
  return (playersOnField > 0);
}

void playGame(int semId, struct GameState *gameState, struct PlayerState *playersState, struct Map *map)
{
  while (gameLoop(semId, gameState, map->field, playersState, map->rowVersions, &map->mapVersion))
  {
    usleep(1000000 / TICKRATE);
  }
}
