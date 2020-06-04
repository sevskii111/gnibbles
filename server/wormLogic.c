#include "wormLogic.h"

#include <sys/socket.h>
#include <unistd.h>
#include "stdlib.h"
#include "stdio.h"

#include "sem.h"
#include "../shared.h"
#include "defines.h"
#include "settings.h"
#include "types.h"

void encodeMapRow(char encoded[MAP_SIZE], int map[MAP_SIZE][MAP_SIZE], int r)
{
  for (int i = 0; i < MAP_SIZE; i++)
  {
    if (map[r][i] <= 1)
    {
      encoded[i] = map[r][i];
    }
    else if (map[r][i] < FOOD_OFFSET)
    {
      encoded[i] = (map[r][i] - 2) / MAX_WORM_LEN + 2;
    }
    else
    {
      encoded[i] = -(map[r][i] - FOOD_OFFSET);
    }
  }
}

void disconnect(int sockfd, int semId, struct PlayerState *myState, int ind)
{
  semLock(semId, PLAYERS_SEM_OFFSET + ind);
  myState->connected = 0;
  semUnlock(semId, PLAYERS_SEM_OFFSET + ind);
  close(sockfd);
}

void *wormTask(void *targs)
{
  struct WormThreadArgs *args = targs;
  int sockfd = args->sockfd;
  int semId = args->semId;
  char ind = args->ind;
  struct PlayerState *playersState = args->playersState;
  struct PlayerState *myState = playersState + ind;
  struct GameState *gameState = args->gameState;
  struct Map *map = args->map;
  myState->connected = 1;
  myState->ready = 0;
  semUnlock(semId, PLAYERS_SEM_OFFSET + ind);
  if (!write(sockfd, &ind, sizeof(ind)))
  {
    disconnect(sockfd, semId, myState, ind);
    return NULL;
  }

  while (1)
  {
    struct WaitingState waitingState;
    do
    {
      char status;
      if (!read(sockfd, &status, sizeof(status)))
      {
        disconnect(sockfd, semId, myState, ind);
        return NULL;
      }
      semLock(semId, PLAYERS_SEM_OFFSET + ind);
      if (status)
      {
        myState->ready = 1;
      }
      else
      {
        myState->ready = 0;
      }
      semUnlock(semId, PLAYERS_SEM_OFFSET + ind);

      semLock(semId, GAME_STATE_SEM);
      waitingState.gamePhase = gameState->gamePhase;
      waitingState.playersConnected = gameState->playersConnected;
      waitingState.playersReady = gameState->playersReady;
      semUnlock(semId, GAME_STATE_SEM);
      if (!write(sockfd, &waitingState, sizeof(waitingState)))
      {
        disconnect(sockfd, semId, myState, ind);
        return NULL;
      }
    } while (waitingState.gamePhase == WAITING_FOR_PLAYERS);

    char mapSize = MAP_SIZE;

    if (!write(sockfd, &mapSize, sizeof(mapSize)))
    {
      disconnect(sockfd, semId, myState, ind);
      return NULL;
    }
    struct InGameState inGameState;
    char mapRowBuff[MAP_SIZE];
    int mapRowVersions[MAP_SIZE];
    for (int i = 0; i < MAP_SIZE; i++)
    {
      mapRowVersions[i] = -1;
    }
    int mapVersion = -1;

    do
    {
      semLock(semId, MAP_SEM);
      char status;
      if (map->mapVersion > mapVersion)
      {
        mapVersion = map->mapVersion;
        status = 1;
        if (!write(sockfd, &status, sizeof(status)))
        {
          semUnlock(semId, MAP_SEM);
          disconnect(sockfd, semId, myState, ind);
          return NULL;
        }
        for (int i = 0; i < MAP_SIZE; i++)
        {
          if (map->rowVersions[i] > mapRowVersions[i])
          {
            mapRowVersions[i] = map->rowVersions[i];
            status = 1;
            if (!write(sockfd, &status, sizeof(status)))
            {
              semUnlock(semId, MAP_SEM);
              disconnect(sockfd, semId, myState, ind);
              return NULL;
            }
            encodeMapRow(mapRowBuff, map->field, i);
            if (!write(sockfd, &mapRowBuff, sizeof(mapRowBuff)))
            {
              semUnlock(semId, MAP_SEM);
              disconnect(sockfd, semId, myState, ind);
              return NULL;
            }
          }
          else
          {
            status = 0;
            if (!write(sockfd, &status, sizeof(status)))
            {
              semUnlock(semId, MAP_SEM);
              disconnect(sockfd, semId, myState, ind);
              return NULL;
            }
          }
        }
        semUnlock(semId, MAP_SEM);
      }
      else
      {
        status = 0;
        if (!write(sockfd, &status, sizeof(status)))
        {
          semUnlock(semId, MAP_SEM);
          disconnect(sockfd, semId, myState, ind);
          return NULL;
        }
        semUnlock(semId, MAP_SEM);
      }

      semLock(semId, GAME_STATE_SEM);
      inGameState.gamePhase = gameState->gamePhase;
      inGameState.activeWorm = gameState->activeWorm;
      semUnlock(semId, GAME_STATE_SEM);
      semLock(semId, ind + PLAYERS_SEM_OFFSET);
      inGameState.score = myState->length - START_SNAKE_LEN;
      semUnlock(semId, ind + PLAYERS_SEM_OFFSET);
      if (!write(sockfd, &inGameState, sizeof(inGameState)))
      {
        disconnect(sockfd, semId, myState, ind);
        return NULL;
      }

      char c;
      if (!read(sockfd, &c, sizeof(c)))
      {
        disconnect(sockfd, semId, myState, ind);
        return NULL;
      }

      if (!myState->direction && c && (!myState->prevDirection || myState->prevDirection == c || myState->prevDirection % 2 != c % 2))
      {
        semUnlock(semId, ind + PLAYERS_SEM_OFFSET);
        semLock(semId, GAME_STATE_SEM);
        if (gameState->activeWorm == ind)
        {
          semUnlock(semId, GAME_STATE_SEM);
          semLock(semId, ind + PLAYERS_SEM_OFFSET);
          myState->direction = c;
          semUnlock(semId, ind + PLAYERS_SEM_OFFSET);
        }
        else
        {
          semUnlock(semId, GAME_STATE_SEM);
        }
      }
      else
      {
        semUnlock(semId, ind + PLAYERS_SEM_OFFSET);
      }
    } while (inGameState.gamePhase == IN_PROGRESS);

    printf("WORM scoreboard\n");
    char sync = 1;
    struct ScoreboardState scoreboardState;
    semLock(semId, GAME_STATE_SEM);
    scoreboardState.gamePhase = gameState->gamePhase;
    scoreboardState.players = gameState->playersReady;
    if (!write(sockfd, &scoreboardState, sizeof(scoreboardState)))
    {
      semUnlock(semId, GAME_STATE_SEM);
      disconnect(sockfd, semId, myState, ind);
      return NULL;
    }
    semUnlock(semId, GAME_STATE_SEM);
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
      semLock(semId, PLAYERS_SEM_OFFSET + i);
      if (playersState[i].length > 0)
      {
        struct ScoreboardRecord record;
        record.ind = i;
        record.score = playersState[i].length - START_SNAKE_LEN;
        if (!write(sockfd, &record, sizeof(record)))
        {
          semUnlock(semId, PLAYERS_SEM_OFFSET + i);
          disconnect(sockfd, semId, myState, ind);
          return NULL;
        }
      }
      semUnlock(semId, PLAYERS_SEM_OFFSET + i);
    }
    if (!read(sockfd, &sync, sizeof(sync)))
    {
      disconnect(sockfd, semId, myState, ind);
      return NULL;
    }

    while (scoreboardState.gamePhase == SCOREBOARD)
    {
      semLock(semId, GAME_STATE_SEM);
      scoreboardState.gamePhase = gameState->gamePhase;
      scoreboardState.players = gameState->playersReady;
      if (!write(sockfd, &scoreboardState, sizeof(scoreboardState)))
      {
        semUnlock(semId, GAME_STATE_SEM);
        disconnect(sockfd, semId, myState, ind);
        return NULL;
      }
      semUnlock(semId, GAME_STATE_SEM);
      if (!read(sockfd, &sync, sizeof(sync)))
      {
        disconnect(sockfd, semId, myState, ind);
        return NULL;
      }
    }
  }
}
