#ifndef DGB_CONCURRENCY_H_
#define DGB_CONCURRENCY_H_

#include <condition_variable>
#include <mutex>


class WaitGroup {
 public:
  void Add(int count) {
    std::lock_guard<std::mutex> lock(mutex_);
    counter_ += count;
  }

  void Done() {
    std::lock_guard<std::mutex> lock(mutex_);
    counter_--;
  }

  void Wait() {
    std::unique_lock<std::mutex> lock(mutex_);
    cond_.wait(lock, [this]{ return this->counter_ == 0; });
  }

 private:
  std::condition_variable cond_;
  std::mutex mutex_;
  int counter_ = 0;
};

#endif  // DGB_CONCURRENCY_H_
