#include "sem.h"

#include "stdio.h"
#include <sys/sem.h>

void sem(int semId, int n, int d)
{
  struct sembuf op;
  op.sem_op = d;
  op.sem_flg = 0;
  op.sem_num = n;
  semop(semId, &op, 1);
}

void semUnlock(int semId, int n)
{
  //printf("u-%d-%d\n", semId, n);
  sem(semId, n, 1);
}

void semLock(int semId, int n)
{
  //printf("l-%d-%d\n", semId, n);
  sem(semId, n, -1);
}