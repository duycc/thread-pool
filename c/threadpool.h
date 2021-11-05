
#if !defined(THREAD_POOL_H)
#define THREAD_POOL_H

#include <pthread.h>

// 任务结构体
typedef struct Task {
  void (*function)(void *param);
  void *arg;
} Task;

// 线程池定义
typedef struct ThreadPool {
  // 任务队列
  Task *taskQue;
  int queCapacity; // 容量
  int queSize;     // 当前任务个数
  int queFront;    // 队头 -> 取数据
  int queBack;     // 队尾 -> 放数据

  pthread_t managerID;       // 管理者线程ID
  pthread_t *threadIDs;      // 工作的线程ID
  int minNum;                // 最小线程数量
  int maxNum;                // 最大线程数量
  int busyNum;               // 忙碌线程数量
  int aliveNum;              // 存活线程数量
  int exitNum;               // 要销毁的线程数量
  pthread_mutex_t mutexPool; // 线程池互斥锁
  pthread_mutex_t mutexBusy; // busyNum锁
  pthread_cond_t notFull;    // 任务队列是否满了
  pthread_cond_t notEmpty;   // 任务队列是否为空

  int shutdown; // 是否销毁线程池： 1 销毁, 0 不销毁
} ThreadPool;

// 创建线程池并初始化
ThreadPool *threadPoolCreate(int min, int max, int queSize);

// 销毁线程池
int threadPoolDestroy(ThreadPool *pool);

// 线程池中添加任务
void threadPoolAdd(ThreadPool *pool, void (*func)(void *), void *arg);

// 获取线程池中工作线程个数
int threadPoolBusyNum(ThreadPool *pool);

// 获取线程池中存活线程个数
int threadPoolAliveNum(ThreadPool *pool);

// 工作的线程（消费者线程）任务函数
void *worker(void *param);

// 管理者线程任务函数
void *manager(void *param);

// 单个线程退出
void threadExit(ThreadPool *pool);

#endif // THREAD_POOL_H
