/**
 * @file     test.cpp
 * @brief
 * @author   DuYong
 * @date     2021-09-01
 */
#include "src/thread_pool.h"
#include <string>

using namespace utils;

void testThreadPool() {
  std::cout << __func__ << std::endl;

  ThreadPool pool(ThreadPool::ThreadPoolConfig{4, 5, 6, std::chrono::seconds(4)});
  pool.start();

  std::this_thread::sleep_for(std::chrono::seconds(4));
  std::cout << "thread size: " << pool.getTotalThreadSize() << std::endl;

  std::atomic<int> index;
  index.store(0);
  std::thread t([&]() {
    for (int i = 0; i < 10; ++i) {
      pool.run([&]() {
        std::cout << "function: " << index.load() << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(4));
        index++;
      });
    }
  });
  t.detach();

  std::cout << std::string(20, '-') << std::endl;

  std::this_thread::sleep_for(std::chrono::seconds(4));
  pool.reset(ThreadPool::ThreadPoolConfig{4, 4, 6, std::chrono::seconds(4)});
  std::this_thread::sleep_for(std::chrono::seconds(4));

  std::cout << "thread size: " << pool.getTotalThreadSize() << std::endl;
  std::cout << "waiting size: " << pool.getWaitingThreadSize() << std::endl;

  std::cout << std::string(20, '-') << std::endl;
  pool.shutDownNow();
  return;
}

int main() {
  testThreadPool();
  return 0;
}
