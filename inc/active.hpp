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
    void run() {
        while (!done) { // note: last message sets done to true
            std::lock_guard<std::mutex> lock(mtx);
            if (!mq.empty()) {
                Message msg = mq.front();
                mq.pop_front();
                msg(); // execute message
            }
        }
    }

  public:
    Active() : done(false) {
        thd = std::unique_ptr<std::thread>(new std::thread([this] { this->run(); }));
    }

    ~Active() {
        send([&] { done = true; });
        thd->join();
    }

    void send(Message m) {
        std::lock_guard<std::mutex> lock(mtx);
        mq.push_back(m);
    }

    Active(const Active &);                  // no copying
    void operator=(const Active &) = delete; // no copying
    bool done;                               // the termination flag
    std::deque<Message> mq;                  // the queue
    std::unique_ptr<std::thread> thd;        // the thread
    std::mutex mtx; // to make deque thread_safe
};