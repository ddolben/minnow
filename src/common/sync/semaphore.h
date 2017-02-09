#ifndef COMMON_SEMAPHORE_H_
#define COMMON_SEMAPHORE_H_

#include <condition_variable>
#include <mutex>


// Semaphore implements a classic semaphore synchronization primitive.
class Semaphore {
 public:
  Semaphore(int count = 0) : count_(count) {}

  inline void Notify() {
    std::unique_lock<std::mutex> lock(mu_);
    // TODO: check for overflow
    count_++;
    cv_.notify_one();
  }

  inline void Wait() {
    std::unique_lock<std::mutex> lock(mu_);
    while(count_ == 0){ cv_.wait(lock); }
    count_--;
  }

 private:
  std::mutex mu_;
  std::condition_variable cv_;
  int count_;
};

#endif  // COMMON_SEMAPHORE_H_
