#ifndef POOL_H
#define POOL_H

#include "wqueue.h"
#include <thread>
#include <vector>
#include <functional>
#include <atomic>

using Job = std::function<void()>;

// Fixed pool of worker threads pulling jobs off a shared bounded queue.
class Pool {
public:
    Pool(int n, size_t cap) : q(cap), done(0), stopped(false) {
        for (int i = 0; i < n; i++)
            workers.emplace_back(&Pool::loop, this);
    }

    ~Pool() { stop(); }

    // Blocks if the queue is full, which is the backpressure on producers.
    bool submit(Job j) { return q.push(std::move(j)); }

    // Closes the queue, lets workers drain what's left, then joins them.
    void stop() {
        if (stopped) return;
        stopped = true;
        q.close();
        for (auto &t : workers)
            if (t.joinable()) t.join();
    }

    int completed() const { return done.load(); }

private:
    void loop() {
        Job j;
        while (q.pop(j)) {   // returns false only when closed and empty
            j();
            done++;
        }
    }

    WQueue<Job> q;
    std::vector<std::thread> workers;
    std::atomic<int> done;
    bool stopped;
};

#endif
