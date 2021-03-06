
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "threadpool.h"

ThreadPool *threadPoolCreate(int min, int max, int queCapacity) {
  ThreadPool *pool = (ThreadPool *)malloc(sizeof(ThreadPool));
  do {
    if (pool == NULL) {
      printf("malloc threadpool fail...\n");
      break;
    }

    pool->threadIDs = (pthread_t *)malloc(sizeof(pthread_t) * max);
    if (pool->threadIDs == NULL) {
      printf("malloc threadIDs fail...\n");
      break;
    }

    memset(pool->threadIDs, 0, sizeof(pthread_t) * max);
    pool->minNum = min;
    pool->maxNum = max;
    pool->busyNum = 0;
    pool->aliveNum = min; // 和最小个数相等
    pool->exitNum = 0;

    if (pthread_mutex_init(&pool->mutexPool, NULL) != 0 || pthread_mutex_init(&pool->mutexBusy, NULL) != 0 ||
        pthread_cond_init(&pool->notEmpty, NULL) != 0 || pthread_cond_init(&pool->notFull, NULL) != 0) {
      printf("mutex or condition init fail...\n");
      break;
    }

    // 任务队列
    pool->taskQue = (Task *)malloc(sizeof(Task) * queCapacity);
    pool->queCapacity = queCapacity;
    pool->queSize = 0;
    pool->queFront = 0;
    pool->queBack = 0;

    pool->shutdown = 0;

    // 创建线程
    // TODO: 返回值没有处理
    pthread_create(&pool->managerID, NULL, manager, pool);
    for (int i = 0; i < min; ++i) {
      pthread_create(&pool->threadIDs[i], NULL, worker, pool);
    }
    return pool;
  } while (0);

  // 资源释放
  if (pool && pool->threadIDs) {
    free(pool->threadIDs);
  }
  if (pool && pool->taskQue) {
    free(pool->taskQue);
  }
  if (pool) {
    free(pool);
  }
  return NULL;
}

int threadPoolDestroy(ThreadPool *pool) {
  if (pool == NULL) {
    return -1;
  }
  // 关闭线程池
  pool->shutdown = 1;
  // 等待管理线程结束
  pthread_join(pool->managerID, NULL);
  // 唤醒阻塞的消费者线程
  for (int i = 0; i < pool->aliveNum; ++i) {
    pthread_cond_signal(&pool->notEmpty);
  }

  // 释放内存
  if (pool->taskQue) {
    free(pool->taskQue);
  }
  if (pool->threadIDs) {
    free(pool->threadIDs);
  }

  pthread_mutex_destroy(&pool->mutexPool);
  pthread_mutex_destroy(&pool->mutexBusy);
  pthread_cond_destroy(&pool->notEmpty);
  pthread_cond_destroy(&pool->notFull);

  free(pool);
  pool = NULL;

  return 0;
}

void threadPoolAdd(ThreadPool *pool, void (*func)(void *), void *arg) {
  pthread_mutex_lock(&pool->mutexPool);
  while (pool->queSize == pool->queCapacity && !pool->shutdown) {
    // 阻塞生产者线程
    pthread_cond_wait(&pool->notFull, &pool->mutexPool);
  }
  if (pool->shutdown) {
    pthread_mutex_unlock(&pool->mutexPool);
    return;
  }

  // 添加任务
  pool->taskQue[pool->queBack].function = func;
  pool->taskQue[pool->queBack].arg = arg;
  pool->queBack = (pool->queBack + 1) % pool->queCapacity;
  pool->queSize++;

  pthread_cond_signal(&pool->notEmpty);
  pthread_mutex_unlock(&pool->mutexPool);
}

int threadPoolBusyNum(ThreadPool *pool) {
  pthread_mutex_lock(&pool->mutexBusy);
  int busyNum = pool->busyNum;
  pthread_mutex_unlock(&pool->mutexBusy);
  return busyNum;
}

int threadPoolAliveNum(ThreadPool *pool) {
  pthread_mutex_lock(&pool->mutexPool);
  int aliveNum = pool->aliveNum;
  pthread_mutex_unlock(&pool->mutexPool);
  return aliveNum;
}

void *worker(void *param) {
  ThreadPool *pool = (ThreadPool *)param;
  while (1) {
    pthread_mutex_lock(&pool->mutexPool);
    // 当前任务队列是否为空
    while (pool->queSize == 0 && !pool->shutdown) {
      // 阻塞工作线程
      pthread_cond_wait(&pool->notEmpty, &pool->mutexPool);

      // 判断是不是要销毁线程
      if (pool->exitNum > 0) {
        pool->exitNum--;
        if (pool->aliveNum > pool->minNum) {
          pool->aliveNum--;
          pthread_mutex_unlock(&pool->mutexPool);
          threadExit(pool);
        }
      }
    }

    // 判断线程池是否关闭
    if (pool->shutdown) {
      pthread_mutex_unlock(&pool->mutexPool);
      threadExit(pool);
    }

    // 从队列中取出一个任务
    Task task;
    task.function = pool->taskQue[pool->queFront].function;
    task.arg = pool->taskQue[pool->queFront].arg;

    // 移动头结点
    pool->queFront = (pool->queFront + 1) % pool->queCapacity;
    pool->queSize--;

    // 解锁
    pthread_cond_signal(&pool->notFull);
    pthread_mutex_unlock(&pool->mutexPool);

    printf("thread %ld start working...\n", pthread_self());
    pthread_mutex_lock(&pool->mutexBusy);
    pool->busyNum++;
    pthread_mutex_unlock(&pool->mutexBusy);
    task.function(task.arg);
    free(task.arg);
    task.arg = NULL;

    printf("thread %ld end working...\n", pthread_self());
    pthread_mutex_lock(&pool->mutexBusy);
    pool->busyNum--;
    pthread_mutex_unlock(&pool->mutexBusy);
  }
  return NULL;
}

#define NUMBER 5

void *manager(void *param) {
  ThreadPool *pool = (ThreadPool *)param;
  while (!pool->shutdown) {
    sleep(3); // 每隔3s检测一次

    // 取出线程池中任务的数量和当前线程的数量
    pthread_mutex_lock(&pool->mutexPool);
    int queSize = pool->queSize;
    int aliveNum = pool->aliveNum;
    pthread_mutex_unlock(&pool->mutexPool);

    // 取出忙的线程数量
    pthread_mutex_lock(&pool->mutexBusy);
    int busyNum = pool->busyNum;
    pthread_mutex_unlock(&pool->mutexBusy);

    // 添加线程：任务个数 > 存活线程个数 && 存活线程数 < 最大线程数
    if (queSize > aliveNum && aliveNum < pool->maxNum) {
      pthread_mutex_lock(&pool->mutexPool);
      int counter = 0;
      for (int i = 0; i < pool->maxNum && counter < NUMBER && pool->aliveNum < pool->maxNum; ++i) {
        if (pool->threadIDs[i] == 0) {
          pthread_create(&pool->threadIDs[i], NULL, worker, pool);
          counter++;
          pool->aliveNum++;
        }
      }
      pthread_mutex_unlock(&pool->mutexPool);
    }

    // 销毁线程：忙碌线程 * 2 < 存活线程数 && 存活线程数 < 最小线程数
    if (busyNum * 2 < aliveNum && aliveNum > pool->minNum) {
      pthread_mutex_lock(&pool->mutexPool);
      pool->exitNum = NUMBER;
      pthread_mutex_unlock(&pool->mutexPool);

      for (int i = 0; i < NUMBER; ++i) {
        pthread_cond_signal(&pool->notEmpty);
      }
    }
  }

  return NULL;
}

void threadExit(ThreadPool *pool) {
  pthread_t tid = pthread_self();
  for (int i = 0; i < pool->maxNum; ++i) {
    if (pool->threadIDs[i] == tid) {
      pool->threadIDs[i] = 0;
      printf("threadExit() called, %ld exiting...\n", tid);
      break;
    }
  }
  pthread_exit(NULL);
}
