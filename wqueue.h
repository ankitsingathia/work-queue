#ifndef WQUEUE_H
#define WQUEUE_H

#include <queue>
#include <mutex>
#include <condition_variable>

// Bounded blocking queue shared between producer and worker threads.
template <class T>
class WQueue {
public:
    explicit WQueue(size_t cap) : cap(cap), closed(false) {}

    // Blocks while the queue is full. Returns false if the queue was closed.
    bool push(T item) {
        std::unique_lock<std::mutex> lk(mu);
        notFull.wait(lk, [this] { return q.size() < cap || closed; });
        if (closed) return false;
        q.push(std::move(item));
        lk.unlock();
        notEmpty.notify_one();
        return true;
    }

    // Blocks while the queue is empty. Returns false once closed and drained.
    bool pop(T &out) {
        std::unique_lock<std::mutex> lk(mu);
        notEmpty.wait(lk, [this] { return !q.empty() || closed; });
        if (q.empty()) return false;
        out = std::move(q.front());
        q.pop();
        lk.unlock();
        notFull.notify_one();
        return true;
    }

    // Wakes every blocked thread so they can exit.
    void close() {
        {
            std::lock_guard<std::mutex> lk(mu);
            closed = true;
        }
        notEmpty.notify_all();
        notFull.notify_all();
    }

    size_t size() {
        std::lock_guard<std::mutex> lk(mu);
        return q.size();
    }

private:
    std::queue<T> q;
    std::mutex mu;
    std::condition_variable notEmpty, notFull;
    size_t cap;
    bool closed;
};

#endif
