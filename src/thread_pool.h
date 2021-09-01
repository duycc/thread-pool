/**
 * @file     thread_pool.h
 * @brief
 * @author   DuYong
 * @date     2021-08-31
 */
#ifndef __THREAD_POOL__
#define __THREAD_POOL__

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <iostream>
#include <list>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <tuple>
#include <utility>

namespace utils {

class ThreadPool {
  using PoolSeconds = std::chrono::seconds;

public:
  struct ThreadPoolConfig {
    int core_threads; // 核心线程个数，线程池中最少拥有的线程个数，初始化就会创建好的线程，常驻于线程池
    int max_threads; // 当 core_threads 线程个数无法满足任务需求时，需要创建额外的线程，总个数不超过 max_threads
    int max_task_size; // 允许存储的最大任务个数
    PoolSeconds time_out; // core_threads 个数以外的线程如果在 time_out 时间内没有执行任务，回收该线程
  };

  // 线程状态
  enum class ThreadState { kInit = 0, kWaiting = 1, kRunning = 2, kStop = 3 };

  // 线程种类
  enum class ThreadFlag { kInit = 0, kCore = 1, kCache = 2 };

  using ThreadPtr = std::shared_ptr<std::thread>;
  using ThreadId = std::atomic<int>;
  using ThreadStateAtomic = std::atomic<ThreadState>;
  using ThreadFlagAtomic = std::atomic<ThreadFlag>;

  struct ThreadWrapper {
    ThreadPtr ptr;
    ThreadId id;
    ThreadStateAtomic state;
    ThreadFlagAtomic flag;

    ThreadWrapper() {
      ptr = nullptr;
      id = 0;
      state.store(ThreadState::kInit);
    }
  };

  using ThreadWrapperPtr = std::shared_ptr<ThreadWrapper>;
  using ThreadPoolLock = std::unique_lock<std::mutex>;

  ThreadPool(ThreadPoolConfig config);
  ~ThreadPool();

  // 开启线程池
  bool start();

  // 退出线程池
  void shutDown();
  void shutDownNow();

  bool reset(ThreadPoolConfig config);

  int getWaitingThreadSize() const;
  int getTotalThreadSize() const;
  int getRunnedFuncNum() const;

  // 放在线程中执行函数
  template <typename F, typename... Args>
  auto run(F &&f, Args &&...args) -> std::shared_ptr<std::future<std::result_of_t<F(Args...)>>> {
    if (this->is_shutdown_.load() || this->is_shutdown_now_.load() || !isAvailable()) {
      return nullptr;
    }
    if (getWaitingThreadSize() == 0 && getTotalThreadSize() < config_.max_threads) {
      addThread(getNextThreadId(), ThreadFlag::kCache);
    }

    using return_type = std::result_of_t<F(Args...)>;
    auto task =
        std::make_shared<std::packaged_task<return_type()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    total_function_num_++;

    std::future<return_type> res = task->get_future();
    {
      ThreadPoolLock lock(this->task_mutex_);
      this->tasks_.emplace([task]() { (*task)(); });
    }
    this->task_cv_.notify_one();
    return std::make_shared<std::future<std::result_of_t<F(Args...)>>>(std::move(res));
  }

private:
  ThreadPoolConfig config_;

  std::list<ThreadWrapperPtr> worker_threads_;

  std::queue<std::function<void()>> tasks_;
  std::mutex task_mutex_;
  std::condition_variable task_cv_;

  std::atomic<int> total_function_num_;
  std::atomic<int> waiting_thread_num_;
  std::atomic<int> thread_id_; // 用于为新创建线程分配 ID

  std::atomic<bool> is_shutdown_now_; // 立即关闭
  std::atomic<bool> is_shutdown_;     // 执行完队列剩余任务后线程关闭
  std::atomic<bool> is_available_;

  bool isInvalidConfig(ThreadPoolConfig &config) const;

  bool isAvailable() const;

  int getNextThreadId();

  void addThread(int id);
  void addThread(int id, ThreadFlag thread_flag);

  void shutDown(bool is_now);

  void resize(int thread_num);
};

} // namespace utils

#endif // __THREAD_POOL__
