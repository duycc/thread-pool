
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "threadpool.h"

void taskFunc(void *param) {
  int num = *(int *)param;
  printf("thread %ld is working, number = %d\n", pthread_self(), num);
  sleep(1);
}

void testThreadPool() {
  ThreadPool *pool = threadPoolCreate(3, 10, 100);
  for (int i = 0; i < 100; ++i) {
    int *num = (int *)malloc(sizeof(int));
    *num = i + 100;
    threadPoolAdd(pool, taskFunc, num);
  }

  sleep(30);

  threadPoolDestroy(pool);
}

int main(int argc, char *argv[]) {
  printf("main start...\n");

  testThreadPool();

  printf("main end...\n");
  return 0;
}
