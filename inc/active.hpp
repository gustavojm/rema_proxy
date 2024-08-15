#pragma once

#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <utility>

class Active {
  public:
    typedef std::function<void()> Message;

  private:
    void run(std::stop_token stop_token) {
        while (!stop_token.stop_requested()) { 
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [this] { return !mq.empty(); });      // During the wait the mutex is unlocked
                                                                // The mutex is automatically re-acquired here
            Message msg = mq.front();                           
            mq.pop_front();
            lock.unlock();
            msg(); // execute message
        }
    }

  public:
    Active() {
        thd = std::jthread(&Active::run, this);
    }

    void send(Message m) {
        {
            std::lock_guard<std::mutex> lock(mtx);        
            mq.push_back(m);
        }
        cv.notify_one();
    }

    Active(const Active &) = delete;         // no copying
    void operator=(const Active &) = delete; // no copying
    std::deque<Message> mq;                  // the queue
    std::jthread thd;                        // the thread
    std::mutex mtx;                          // to make deque thread_safe
    std::condition_variable cv;
};