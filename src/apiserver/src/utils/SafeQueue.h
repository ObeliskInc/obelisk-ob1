// Copyright 2018 Obelisk Inc.

#ifndef SAFEQUEUE_H
#define SAFEQUEUE_H

#include <condition_variable>
#include <mutex>
#include <queue>

using namespace std;

// A threadsafe-queue.
template <class T> class SafeQueue {
public:
  SafeQueue() : q(), m(), c() {}

  ~SafeQueue() {}

  // Add an element to the queue.
  void enqueue(T t) {
    std::lock_guard<std::mutex> lock(m);
    q.push(t);
    c.notify_one();
    c.notify_one();
    std::thread::id this_id = std::this_thread::get_id();
  }

  // Get the "front"-element.
  // If the queue is empty, wait till a element is available.
  T dequeue() {
    std::unique_lock<std::mutex> lock(m);
    while (q.empty()) {
      // release lock as long as the wait and reacquire it afterwards.
      std::thread::id this_id = std::this_thread::get_id();
      c.wait(lock);
    }
    T val = q.front();
    q.pop();
    return val;
  }

  bool isEmpty() {
    std::lock_guard<std::mutex> lock(m);
    bool e = q.empty();
    return e;
  }

private:
  std::queue<T> q;
  mutable std::mutex m;
  std::condition_variable c;
};

#endif
