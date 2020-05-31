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
#include "../shared.h"
#include "sem.c"

#define MAP_SIZE 20
#define MAX_PLAYERS 8

struct GlobalState
{
  char gamePhase;
  char playersConnected;
  char playersReady;
};

struct PlayerState
{
  char ind;
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

struct WormThreadArgs
{
  int sockfd;
  int semId;
  struct PlayeState *state;
  struct GlobalState *globalState;
};

void *wormTask(void *targs)
{
  struct WormThreadArgs *args = (struct WormThreadArgs *)targs;
  int sockfd = args->sockfd;
  int semId = args->semId;
  struct PlayerState *myState = args->state;
  struct GlobalState *globalState = args->globalState;
  free(targs);
  lockSem(semId, 1 + myState->ind);
  myState->connected = 1;
  myState->ready = 0;
  unlockSem(semId, 1 + myState->ind);
  int s = write(sockfd, &myState->ind, sizeof(myState->ind));
  if (s == 0)
  {
    lockSem(semId, 1 + myState->ind);
    myState->connected = 0;
    unlockSem(semId, 1 + myState->ind);
    close(sockfd);
    return NULL;
  }

  printf("New player connected(%d)\n", myState->ind);
  while (1)
  {
    printf("Serving request\n");
    char status;
    int n = read(sockfd, &status, sizeof(status));
    if (n == 0)
    {
      lockSem(semId, 1 + myState->ind);
      myState->connected = 0;
      unlockSem(semId, 1 + myState->ind);
      lockSem(semId, 0);
      close(sockfd);
      return NULL;
    }
    lockSem(semId, 1 + myState->ind);
    if (status)
    {
      myState->ready = 1;
    }
    else
    {
      myState->ready = 0;
    }
    unlockSem(semId, 1 + myState->ind);

    lockSem(semId, 0);
    n = write(sockfd, globalState, sizeof(globalState));
    printf("Sent state:\n%d players\n%d ready\n%d gamePhase\n", globalState->playersConnected, globalState->playersReady, globalState->gamePhase);
    if (globalState->gamePhase != WAITING_FOR_PLAYERS)
    {
      unlockSem(semId, 0);
      break;
    }
    unlockSem(semId, 0);
  }

  while (1)
  {
    sleep(1);
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
  for (int i = 0; i < MAX_PLAYERS; i++)
  {
    playersState[i].ind = i;
  }

  int sockfd = setUpServer(argv[1]);
  //0 for global state, 1-8 for snakes
  int semId = semget(IPC_PRIVATE, 9, 0600 | IPC_CREAT);
  for (int i = 0; i < 9; i++)
  {
    unlockSem(semId, i);
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
          struct WormThreadArgs *newWormThreadArgs = malloc(sizeof(newWormThreadArgs));
          newWormThreadArgs->sockfd = newsockfd;
          newWormThreadArgs->state = (playersState + i);
          newWormThreadArgs->globalState = globalState;
          newWormThreadArgs->semId = semId;
          pthread_create(&newWormThread, NULL, wormTask, (void *)newWormThreadArgs);
          break;
        }
      }
    }

    lockSem(semId, 0);
    globalState->playersConnected = 0;
    globalState->playersReady = 0;
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
      lockSem(semId, 1 + i);
      if (playersState[i].connected)
      {
        globalState->playersConnected++;
      }
      if (playersState[i].ready)
      {
        globalState->playersReady++;
      }
      unlockSem(semId, 1 + i);
    }
    if (globalState->playersConnected > 0 && globalState->playersConnected == globalState->playersReady)
    {
      globalState->gamePhase = IN_PROGRESS;
      unlockSem(semId, 0);
      break;
    }
    unlockSem(semId, 0);

    sleep(1);
  }
  printf("All players are ready!\n");
  while (1)
  {
    sleep(1);
  }
}