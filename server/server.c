#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <pthread.h>
#include <fcntl.h>
#include "sem.c"
#include "../shared.h"

#define MAP_SIZE 20
#define MAX_PLAYERS 8
#define START_SNAKE_LEN 3
#define GLOBAL_STATE_SEM 0
#define MAP_SEM 1
#define PLAYERS_SEM_OFFSET 2

const int MAX_WORM_LEN = (MAP_SIZE * MAP_SIZE);
const int FOOD_OFFSET = 2 + (MAP_SIZE * MAP_SIZE) * (MAX_PLAYERS - 1);
const int FOOD_TARGET = (MAP_SIZE * MAP_SIZE) / 50;

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

int map[MAP_SIZE][MAP_SIZE];
struct PlayerState playersState[MAX_PLAYERS];

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

  map[1][1] = 2 + MAX_WORM_LEN * 0;
  map[1][18] = 2 + MAX_WORM_LEN * 1;
  map[18][1] = 2 + MAX_WORM_LEN * 2;
  map[18][18] = 2 + MAX_WORM_LEN * 3;
  map[10][1] = 2 + MAX_WORM_LEN * 4;
  map[1][10] = 2 + MAX_WORM_LEN * 5;
  map[10][18] = 2 + MAX_WORM_LEN * 6;
  map[18][10] = 2 + MAX_WORM_LEN * 7;
}

int setUpServer(char *port)
{
  int sockfd;
  struct sockaddr_in servaddr;

  bzero(&servaddr, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(atoi(port));
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

  sockfd = socket(PF_INET, SOCK_STREAM, 0);
  if (bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
  {
    servaddr.sin_port = 0;
    if (bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
      perror(NULL);
      close(sockfd);
      exit(1);
    }
  }
  socklen_t servlen = sizeof(servaddr);
  listen(sockfd, 5);
  getsockname(sockfd, (struct sockaddr *)&servaddr, &servlen);
  printf("Listening on port: %d\n", ntohs(servaddr.sin_port));
  fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL, 0) | O_NONBLOCK);

  return sockfd;
}

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

struct WormThreadArgs
{
  int sockfd;
  int semId;
  char ind;
  struct PlayeState *state;
  struct GameState *gameState;
};

void gameLoop(int semId, struct GameState *gameState, int map[20][20], struct PlayerState playerStates[MAX_PLAYERS])
{
  for (int i = 0; i < MAX_PLAYERS; i++)
  {
    semLock(semId, i + PLAYERS_SEM_OFFSET);
  }
  semLock(semId, MAP_SEM);
  semLock(semId, GLOBAL_STATE_SEM);

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
        break;
      }
    }
  }

  if (playersOnField == 0)
  {
    gameState->gamePhase = SCOREBOARD;
    return;
  }

  char toRemoveC = 0;
  char toRemove[MAX_PLAYERS];
  if (allPlayersMadeTurn)
  {
    char lenIncrease[MAX_PLAYERS];
    bzero(lenIncrease, sizeof(lenIncrease));
    char targetX[MAX_PLAYERS], targetY[MAX_PLAYERS];
    char foodOnMap = 0;
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
          map[i][j]++;
          if (segmentInd == 0)
          {
            char direction = playersState[snakeId].direction;
            if (direction == 1)
            {
              targetX[snakeId] = i;
              targetY[snakeId] = j - 1;
            }
            else if (direction == 2)
            {
              targetX[snakeId] = i + 1;
              targetY[snakeId] = j;
            }
            else if (direction == 3)
            {
              targetX[snakeId] = i;
              targetY[snakeId] = j + 1;
            }
            else if (direction == 4)
            {
              targetX[snakeId] = i - 1;
              targetY[snakeId] = j;
            }

            if (map[targetX[snakeId]][targetY[snakeId]])
            {
              if (map[targetX[snakeId]][targetY[snakeId]] < FOOD_OFFSET)
              {
                if (map[targetX[snakeId]][targetY[snakeId]] > 1)
                {
                  int targetSnakeId = (map[targetX[snakeId]][targetY[snakeId]] - 2) / MAX_WORM_LEN;
                  int targetSegmentId = (map[targetX[snakeId]][targetY[snakeId]] - 2) % MAX_WORM_LEN;
                  if (targetSegmentId + 1 >= playersState[targetSnakeId].length)
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
                lenIncrease[snakeId] = map[targetX[snakeId]][targetY[snakeId]] - FOOD_OFFSET;
                map[targetX[snakeId]][targetY[snakeId]] = 2 + MAX_WORM_LEN * i;
              }
            }
          }
          else if (segmentInd + 1 >= playersState->length)
          {
            map[i][j] = 0;
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

        map[targetX[i]][targetY[i]] = 2 + MAX_WORM_LEN * i;
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
      int n = rand() % 9;
      map[x][y] = FOOD_OFFSET + n;
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
        int n = rand() % 9;
        map[i][j] = FOOD_OFFSET + n;
        gameState->foodOnMap++;
      }
    }
  }

  for (int i = 0; i < MAX_PLAYERS; i++)
  {
    semUnlock(semId, i + PLAYERS_SEM_OFFSET);
  }
  semUnlock(semId, MAP_SEM);
  semUnlock(semId, GLOBAL_STATE_SEM);
}

void *wormTask(void *targs)
{
  struct WormThreadArgs *args = targs;
  int sockfd = args->sockfd;
  int semId = args->semId;
  char ind = args->ind;
  struct PlayerState *myState = args->state;
  struct GameState *gameState = args->gameState;
  free(targs);
  semLock(semId, PLAYERS_SEM_OFFSET + ind);
  myState->connected = 1;
  myState->ready = 0;
  semUnlock(semId, PLAYERS_SEM_OFFSET + ind);
  int s = write(sockfd, &ind, sizeof(ind));
  if (s == 0)
  {
    semLock(semId, PLAYERS_SEM_OFFSET + ind);
    myState->connected = 0;
    semUnlock(semId, PLAYERS_SEM_OFFSET + ind);
    close(sockfd);
    return NULL;
  }

  printf("New player connected(%d)\n", ind);
  struct WaitingState *waitingState = malloc(sizeof(*waitingState));
  while (1)
  {
    char status;
    int n = read(sockfd, &status, sizeof(status));
    if (n == 0)
    {
      semLock(semId, PLAYERS_SEM_OFFSET + ind);
      myState->connected = 0;
      semUnlock(semId, PLAYERS_SEM_OFFSET + ind);
      close(sockfd);
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

    semLock(semId, GLOBAL_STATE_SEM);

    waitingState->gamePhase = gameState->gamePhase;
    waitingState->playersConnected = gameState->playersConnected;
    waitingState->playersReady = gameState->playersReady;

    n = write(sockfd, waitingState, sizeof(*waitingState));
    if (gameState->gamePhase != WAITING_FOR_PLAYERS)
    {
      semUnlock(semId, GLOBAL_STATE_SEM);
      break;
    }
    semUnlock(semId, GLOBAL_STATE_SEM);
  }
  free(waitingState);

  struct InGameState *inGameState = malloc(sizeof(*inGameState));
  char mapSize = MAP_SIZE;
  char mapRowBuff[MAP_SIZE];
  write(sockfd, &mapSize, sizeof(mapSize));
  while (1)
  {
    semLock(semId, MAP_SEM);
    for (int i = 0; i < MAP_SIZE; i++)
    {
      encodeMapRow(mapRowBuff, map, i);
      write(sockfd, &mapRowBuff, sizeof(mapRowBuff));
    }
    semUnlock(semId, MAP_SEM);
    semLock(semId, GLOBAL_STATE_SEM);
    inGameState->gamePhase = gameState->gamePhase;
    inGameState->activeWorm = gameState->activeWorm;

    write(sockfd, inGameState, sizeof(*inGameState));
    semUnlock(semId, GLOBAL_STATE_SEM);

    char c;
    read(sockfd, &c, sizeof(c));
    if (gameState->activeWorm == ind && !myState->direction && c)
    {
      if (!myState->prevDirection || myState->prevDirection == c || myState->prevDirection % 2 != c % 2)
      {
        myState->direction = c;
        printf("recived %c controll from worm %d\n", c, ind);
      }
    }
  }
}

int main(int argc, char *argv[])
{
  if (argc < 2)
  {
    printf("Please specify port!\n");
    return -1;
  }

  struct GameState *globalState = malloc(sizeof(struct GameState));
  globalState->gamePhase = WAITING_FOR_PLAYERS;
  globalState->activeWorm = 0;
  bzero(&playersState, sizeof(playersState));
  for (int i = 0; i < MAX_PLAYERS; i++)
  {
    playersState[i].length = START_SNAKE_LEN;
  }

  int sockfd = setUpServer(argv[1]);
  loadMap(map);
  //0 for global state, 1 for map, 1-8 for snakes
  int semId = semget(IPC_PRIVATE, PLAYERS_SEM_OFFSET + MAX_PLAYERS, 0600 | IPC_CREAT);
  for (int i = 0; i < PLAYERS_SEM_OFFSET + MAX_PLAYERS; i++)
  {
    semUnlock(semId, i);
  }
  struct sockaddr_in cliaddr;
  socklen_t clilen = sizeof(cliaddr);
  while (1)
  {
    int newsockfd = accept(sockfd, (struct sockaddr *)&cliaddr, &clilen);
    if (newsockfd == 0)
    {
      printf("Socket failed!\n");
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
          newWormThreadArgs->state = (playersState + i);
          newWormThreadArgs->gameState = globalState;
          newWormThreadArgs->semId = semId;
          newWormThreadArgs->ind = (char)i;

          pthread_create(&newWormThread, NULL, wormTask, (void *)newWormThreadArgs);
          break;
        }
      }
    }

    semLock(semId, GLOBAL_STATE_SEM);
    globalState->playersConnected = 0;
    globalState->playersReady = 0;
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
      semLock(semId, PLAYERS_SEM_OFFSET + i);
      if (playersState[i].connected)
      {
        globalState->playersConnected++;
      }
      if (playersState[i].ready)
      {
        globalState->playersReady++;
      }
      semUnlock(semId, PLAYERS_SEM_OFFSET + i);
    }
    if (globalState->playersConnected > 0 && globalState->playersConnected == globalState->playersReady)
    {
      globalState->gamePhase = IN_PROGRESS;
      semUnlock(semId, GLOBAL_STATE_SEM);
      break;
    }
    semUnlock(semId, GLOBAL_STATE_SEM);

    sleep(1);
  }
  printf("All players are ready!\n");
  for (int i = 0; i < MAX_PLAYERS; i++)
  {
    semLock(semId, PLAYERS_SEM_OFFSET + i);
    playersState[i].ready = 1;
    semUnlock(semId, PLAYERS_SEM_OFFSET + i);
  }
  while (1)
  {
    gameLoop(semId, globalState, map, playersState);
    sleep(1);
  }
}