#pragma once

#include <thread>
#include <utility>
#include <functional>
#include <deque>
#include <mutex>

class Active {
  public:
    typedef std::function<void()> Message;

  private:
    Active(const Active &);                     // no copying
    void operator=(const Active &) = delete;    // no copying
    bool done;                                  // the termination flag
    std::deque<Message> mq;                     // the queue
    std::unique_ptr<std::thread> thd;    // le thread
    void Run() {
        while (!done) {
            std::lock_guard<std::mutex> lock(mtx);
            if (!mq.empty()) {
                Message msg = mq.front();
                mq.pop_front();
                msg();      // execute message
            }
        }               // note: last message sets done to true
    }

  public:
    Active() : done(false) {
        thd = std::unique_ptr<std::thread>(new std::thread([this] { this->Run(); }));
    }
    ~Active() {
        Send([&] { done = true; });
        thd->join();
    }
    void Send(Message m) {
        std::lock_guard<std::mutex> lock(mtx);
        mq.push_back(m);
    }

    std::mutex mtx;                             // to make deque thread_safe
};