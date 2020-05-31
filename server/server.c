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
#define GLOBAL_STATE_SEM 0
#define MAP_SEM 1
#define PLAYERS_SEM_OFFSET 2

int map[MAP_SIZE][MAP_SIZE];

struct PlayerState
{
  char connected;
  char ready;
  char direction;
};

void loadMap(char players, int map[MAP_SIZE][MAP_SIZE])
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

  int maxWormLen = (MAP_SIZE * MAP_SIZE);
  map[0][0] = 2 + maxWormLen * 0;
  map[0][19] = 3 + maxWormLen * 1;
  map[19][0] = 4 + maxWormLen * 2;
  map[19][19] = 5 + maxWormLen * 3;
  map[10][0] = 6 + maxWormLen * 4;
  map[0][10] = 7 + maxWormLen * 5;
  map[10][19] = 8 + maxWormLen * 6;
  map[19][10] = 9 + maxWormLen * 7;
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
  int maxWormLen = (MAP_SIZE * MAP_SIZE);
  *encoded = 0;
  for (int i = 0; i < MAP_SIZE; i++)
  {
    int curr;
    if (map[r][i] <= 1)
    {
      curr = map[r][i];
    }
    else
    {
      curr = map[r][i] / maxWormLen;
    }
    encoded[i] = curr;
  }
}

struct WormThreadArgs
{
  int sockfd;
  int semId;
  int ind;
  struct PlayeState *state;
  struct GlobalState *globalState;
};

void *wormTask(void *targs)
{
  struct WormThreadArgs *args = (struct WormThreadArgs *)targs;
  int sockfd = args->sockfd;
  int semId = args->semId;
  int ind = args->ind;
  struct PlayerState *myState = args->state;
  struct GlobalState *globalState = args->globalState;
  free(targs);
  lockSem(semId, PLAYERS_SEM_OFFSET + ind);
  myState->connected = 1;
  myState->ready = 0;
  unlockSem(semId, PLAYERS_SEM_OFFSET + ind);
  int s = write(sockfd, &ind, sizeof(ind));
  if (s == 0)
  {
    lockSem(semId, PLAYERS_SEM_OFFSET + ind);
    myState->connected = 0;
    unlockSem(semId, PLAYERS_SEM_OFFSET + ind);
    close(sockfd);
    return NULL;
  }

  printf("New player connected(%d)\n", ind);
  while (1)
  {
    char status;
    int n = read(sockfd, &status, sizeof(status));
    if (n == 0)
    {
      lockSem(semId, PLAYERS_SEM_OFFSET + ind);
      myState->connected = 0;
      unlockSem(semId, PLAYERS_SEM_OFFSET + ind);
      close(sockfd);
      return NULL;
    }
    lockSem(semId, PLAYERS_SEM_OFFSET + ind);
    if (status)
    {
      myState->ready = 1;
    }
    else
    {
      myState->ready = 0;
    }
    unlockSem(semId, PLAYERS_SEM_OFFSET + ind);

    lockSem(semId, GLOBAL_STATE_SEM);
    n = write(sockfd, globalState, sizeof(globalState));
    if (globalState->gamePhase != WAITING_FOR_PLAYERS)
    {
      unlockSem(semId, GLOBAL_STATE_SEM);
      break;
    }
    unlockSem(semId, GLOBAL_STATE_SEM);
  }

  char mapSize = MAP_SIZE;
  char mapRowBuff[MAP_SIZE];
  write(sockfd, &mapSize, sizeof(mapSize));
  while (1)
  {
    lockSem(semId, MAP_SEM);
    for (int i = 0; i < MAP_SIZE; i++)
    {
      encodeMapRow(mapRowBuff, map, i);
      write(sockfd, &mapRowBuff, sizeof(mapRowBuff));
    }
    lockSem(semId, GLOBAL_STATE_SEM);
    write(sockfd, &globalState, sizeof(globalState));
    unlockSem(semId, GLOBAL_STATE_SEM);
    if (globalState->activeWorm == ind)
    {
      char c;
      read(sockfd, &c, sizeof(c));
      printf("recived %c controll from worm %d\n", c, ind);
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

  struct GlobalState *globalState = malloc(sizeof(globalState));
  globalState->gamePhase = WAITING_FOR_PLAYERS;
  struct PlayerState playersState[MAX_PLAYERS];
  bzero(&playersState, sizeof(playersState));

  int sockfd = setUpServer(argv[1]);
  //0 for global state, 1 for map, 1-8 for snakes
  int semId = semget(IPC_PRIVATE, PLAYERS_SEM_OFFSET + MAX_PLAYERS, 0600 | IPC_CREAT);
  for (int i = 0; i < PLAYERS_SEM_OFFSET + MAX_PLAYERS; i++)
  {
    unlockSem(semId, i);
  }
  lockSem(semId, 2); //Locking map because it is not yet loaded
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
          struct WormThreadArgs *newWormThreadArgs = malloc(sizeof(newWormThreadArgs));
          newWormThreadArgs->sockfd = newsockfd;
          newWormThreadArgs->state = (playersState + i);
          newWormThreadArgs->globalState = globalState;
          newWormThreadArgs->semId = semId;
          newWormThreadArgs->ind = i;
          pthread_create(&newWormThread, NULL, wormTask, (void *)newWormThreadArgs);
          break;
        }
      }
    }

    lockSem(semId, GLOBAL_STATE_SEM);
    globalState->playersConnected = 0;
    globalState->playersReady = 0;
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
      lockSem(semId, PLAYERS_SEM_OFFSET + i);
      if (playersState[i].connected)
      {
        globalState->playersConnected++;
      }
      if (playersState[i].ready)
      {
        globalState->playersReady++;
      }
      unlockSem(semId, PLAYERS_SEM_OFFSET + i);
    }
    if (globalState->playersConnected > 0 && globalState->playersConnected == globalState->playersReady)
    {
      globalState->gamePhase = IN_PROGRESS;
      unlockSem(semId, GLOBAL_STATE_SEM);
      break;
    }
    unlockSem(semId, GLOBAL_STATE_SEM);

    sleep(1);
  }
  printf("All players are ready!\n");
  loadMap(globalState->playersConnected, map);
  unlockSem(semId, MAP_SEM);

  while (1)
  {
    sleep(1);
  }
}