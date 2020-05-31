#include <stdio.h>
#include <sys/sem.h>

void sem(int semId, int n, int d)
{
  struct sembuf op;
  op.sem_op = d;
  op.sem_flg = 0;
  op.sem_num = n;
  semop(semId, &op, 1);
}

void unlockSem(int semId, int n)
{
  sem(semId, n, 1);
}

void lockSem(int semId, int n)
{
  sem(semId, n, -1);
}