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

#define MAP_SIZE 20
#define MAX_PLAYERS 8

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

struct WormThreadArgs
{
  int sockfd;
  int ind;
};

void *wormTask(void *targs)
{
  struct WormThreadArgs *args = (struct WormThreadArgs *)targs;
  int sockfd = args->sockfd;
  char ind = args->ind;
  printf("%d %d\n", sockfd, ind);
  write(sockfd, &ind, sizeof(char));
  while (1)
  {
    int t;
    int tt = read(sockfd, &t, sizeof(int));
    printf("%d %d\n", t, tt);
    printf("%d %d\n", sockfd, ind);
  }
}

int main(int argc, char *argv[])
{
  if (argc < 2)
  {
    printf("Please specify port!\n");
    return -1;
  }

  char players = 0;
  struct PlayerState playersState[MAX_PLAYERS];
  bzero(&playersState, sizeof(playersState));
  int sockfd = setUpServer(argv[1]);
  char openForNewPlayers = 1;

  struct sockaddr_in cliaddr;
  socklen_t clilen = sizeof(cliaddr);
  while (1)
  {
    int newsockfd = accept(sockfd, (struct sockaddr *)&cliaddr, &clilen);
    if (newsockfd == 0)
    {
      printf("Socket failed!");
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
          newWormThreadArgs->ind = i;
          printf("I will make new thread!\n");
          pthread_create(&newWormThread, NULL, wormTask, (void *)newWormThreadArgs);
          break;
        }
      }
    }
    printf("%d\n", newsockfd);
    sleep(1);
  }
}