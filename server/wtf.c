#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

struct thread_args
{
  size_t msgId;
};

int main()
{
  struct thread_args *args;
  args->msgId = (size_t)1;
  return 0;
}