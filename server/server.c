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
#include <sys/sem.h>
#include "sem.h"
#include "../shared.h"
#include "defines.h"
#include "settings.h"
#include "types.h"
#include "serverGameLogic.h"
#include "wormLogic.h"

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

int main(int argc, char *argv[])
{
  if (argc < 2)
  {
    printf("Please specify port!\n");
    return -1;
  }

  int sockfd = setUpServer(argv[1]);

  int semId = semget(IPC_PRIVATE, PLAYERS_SEM_OFFSET + MAX_PLAYERS, 0600 | IPC_CREAT);
  for (int i = 0; i < PLAYERS_SEM_OFFSET + MAX_PLAYERS; i++)
  {
    semUnlock(semId, i);
  }

  struct GameState *gameState = malloc(sizeof(struct GameState));
  struct Map *map = malloc(sizeof(struct Map));
  struct PlayerState *playersState = malloc(sizeof(struct PlayerState) * MAX_PLAYERS);
  bzero(playersState, sizeof(*playersState) * MAX_PLAYERS);

  while (1)
  {
    resetState(semId, gameState, playersState, map);
    if (acceptPlayers(sockfd, semId, gameState, playersState, map) != 0)
    {
      printf("Socket failed!\n");
      return -1;
    }
    printf("All players are ready!\n");
    setupGame(semId, playersState);
    playGame(semId, gameState, playersState, map);
    printf("Game finished\n");
    sleep(SCOREBOARD_TIME);
  }
}