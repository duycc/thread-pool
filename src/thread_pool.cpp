/**
 * @file     thread_pool.cpp
 * @brief
 * @author   DuYong
 * @date     2021-08-31
 */

#include "thread_pool.h"

using namespace utils;

ThreadPool::ThreadPool(ThreadPoolConfig config) : config_(config) {
  this->total_function_num_.store(0);
  this->waiting_thread_num_.store(0);

  this->thread_id_.store(0);
  this->is_shutdown_.store(false);
  this->is_shutdown_now_.store(false);

  if (isInvalidConfig(config_)) {
    is_available_.store(false);
  } else {
    is_available_.store(true);
  }
  return;
}

ThreadPool::~ThreadPool() { shutDown(); }

bool ThreadPool::start() {
  if (isAvailable()) {
    int core_thread_num = config_.core_threads;
    std::cout << "Init thread num: " << core_thread_num << std::endl;
    while (core_thread_num-- > 0) {
      addThread(getNextThreadId());
    }
    std::cout << "Init thread end." << std::endl;
    return true;
  }
  return false;
}

void ThreadPool::addThread(int id) {
  this->addThread(id, ThreadFlag::kCore);
  return;
}
void ThreadPool::addThread(int id, ThreadFlag thread_flag) {
  std::cout << "addThread: " << id << ", flag: " << static_cast<int>(thread_flag) << std::endl;
  ThreadWrapperPtr thread_ptr = std::make_shared<ThreadWrapper>();

  thread_ptr->id.store(id);
  thread_ptr->flag.store(thread_flag);

  auto func = [this, thread_ptr]() {
    while (true) {
      std::function<void()> task;
      {
        ThreadPoolLock lock(this->task_mutex_);
        if (thread_ptr->state.load() == ThreadState::kStop) {
          break;
        }
        std::cout << "thread id: " << thread_ptr->id.load() << " running start." << std::endl;
        thread_ptr->state.store(ThreadState::kWaiting);
        ++this->waiting_thread_num_;
        bool is_timeout = false;
        if (thread_ptr->flag.load() == ThreadFlag::kCore) {
          this->task_cv_.wait(lock, [this, thread_ptr] {
            return (
                this->is_shutdown_ || this->is_shutdown_now_ || !this->tasks_.empty() ||
                thread_ptr->state.load() == ThreadState::kStop);
          });
        } else {
          this->task_cv_.wait_for(lock, this->config_.time_out, [this, thread_ptr] {
            return (
                this->is_shutdown_ || this->is_shutdown_now_ || !this->tasks_.empty() ||
                thread_ptr->state.load() == ThreadState::kStop);
          });
          is_timeout =
              !(this->is_shutdown_ || this->is_shutdown_now_ || !this->tasks_.empty() ||
                thread_ptr->state.load() == ThreadState::kStop);
        }
        --this->waiting_thread_num_;
        std::cout << "thread id: " << thread_ptr->id.load() << " running wait end." << std::endl;

        if (is_timeout) {
          thread_ptr->state.store(ThreadState::kStop);
        }
        if (thread_ptr->state.load() == ThreadState::kStop) {
          std::cout << "thread id: " << thread_ptr->id.load() << " state stop." << std::endl;
          break;
        }
        if (this->is_shutdown_ && this->tasks_.empty()) {
          std::cout << "thread id: " << thread_ptr->id.load() << " shutdown." << std::endl;
          break;
        }
        if (this->is_shutdown_now_) {
          std::cout << "thread id: " << thread_ptr->id.load() << " shutdown now." << std::endl;
          break;
        }
        thread_ptr->state.store(ThreadState::kRunning);
        task = std::move(this->tasks_.front());
        this->tasks_.pop();
      }
      task();
    }
    std::cout << "thread id: " << thread_ptr->id.load() << " running end." << std::endl;
  };
  thread_ptr->ptr = std::make_shared<std::thread>(std::move(func));
  if (thread_ptr->ptr->joinable()) {
    thread_ptr->ptr->detach();
  }
  this->worker_threads_.emplace_back(std::move(thread_ptr));
  return;
}

void ThreadPool::shutDown() {
  this->shutDown(false);
  std::cout << "shutdown." << std::endl;
  return;
}

void ThreadPool::shutDownNow() {
  this->shutDown(true);
  std::cout << "shutdown now." << std::endl;
  return;
}

void ThreadPool::shutDown(bool is_now) {
  if (this->is_available_.load()) {
    if (is_now) {
      this->is_shutdown_now_.store(true);
    } else {
      this->is_shutdown_.store(true);
    }
    this->task_cv_.notify_all();
    this->is_available_.store(false);
  }
  return;
}

bool ThreadPool::isInvalidConfig(ThreadPoolConfig &config) const {
  return config.core_threads < 1U || config.max_threads < config.core_threads || config.time_out.count() < 1;
}

bool ThreadPool::isAvailable() const { return this->is_available_.load(); }

int ThreadPool::getNextThreadId() { return this->thread_id_++; }

int ThreadPool::getWaitingThreadSize() const { return this->waiting_thread_num_.load(); }

int ThreadPool::getTotalThreadSize() const { return this->worker_threads_.size(); }

int ThreadPool::getRunnedFuncNum() const { return total_function_num_.load(); }

bool ThreadPool::reset(ThreadPoolConfig config) {
  if (isInvalidConfig(config)) {
    return false;
  }
  if (config_.core_threads != config.core_threads) {
    return false;
  }
  config_ = config;
  return true;
}

void ThreadPool::resize(int thread_num) {
  if (thread_num < config_.core_threads) {
    return;
  }
  int old_thread_num = worker_threads_.size();
  std::cout << "old num: " << old_thread_num << ", resize: " << thread_num << std::endl;
  if (thread_num > old_thread_num) {
    while (thread_num-- > old_thread_num) {
      addThread(getNextThreadId());
    }
  } else {
    int diff = old_thread_num - thread_num;
    auto iter = worker_threads_.begin();
    while (iter != worker_threads_.end()) {
      if (diff == 0) {
        break;
      }
      auto thread_ptr = *iter;
      if (thread_ptr->flag.load() == ThreadFlag::kCache && thread_ptr->state.load() == ThreadState::kWaiting) {
        thread_ptr->state.store(ThreadState::kStop);
        --diff;
        iter = worker_threads_.erase(iter);
      } else {
        ++iter;
      }
    }
    this->task_cv_.notify_all();
  }
  return;
}
