#include "pool.h"
#include <cstdio>
#include <chrono>
#include <thread>
#include <vector>
#include <atomic>

// Small self-contained test runner, so the tests need nothing beyond the
// standard library, same as the headers.
static int failures = 0;

#define CHECK(cond)                                                  \
    do {                                                             \
        if (!(cond)) {                                               \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            failures++;                                              \
        }                                                            \
    } while (0)

using namespace std::chrono_literals;

// Long enough that a thread which should be blocked would have finished by now.
static const auto SETTLE = 100ms;

static void fifo_order() {
    WQueue<int> q(8);
    for (int i = 1; i <= 5; i++) CHECK(q.push(i));
    CHECK(q.size() == 5);
    for (int i = 1; i <= 5; i++) {
        int v = 0;
        CHECK(q.pop(v));
        CHECK(v == i);
    }
    CHECK(q.size() == 0);
}

// A full queue must hold the producer until a consumer frees a slot.
static void push_blocks_when_full() {
    WQueue<int> q(2);
    q.push(1);
    q.push(2);

    std::atomic<bool> pushed(false);
    std::thread producer([&] {
        q.push(3);
        pushed = true;
    });

    std::this_thread::sleep_for(SETTLE);
    CHECK(!pushed);

    int v = 0;
    q.pop(v);
    producer.join();
    CHECK(pushed);
    CHECK(q.size() == 2);
}

static void pop_blocks_when_empty() {
    WQueue<int> q(2);
    std::atomic<bool> popped(false);
    int got = 0;
    std::thread consumer([&] {
        q.pop(got);
        popped = true;
    });

    std::this_thread::sleep_for(SETTLE);
    CHECK(!popped);

    q.push(42);
    consumer.join();
    CHECK(popped);
    CHECK(got == 42);
}

// close() has to wake threads blocked on either side, or shutdown hangs.
static void close_wakes_blocked_pop() {
    WQueue<int> q(2);
    bool result = true;
    std::thread consumer([&] {
        int v;
        result = q.pop(v);
    });
    std::this_thread::sleep_for(SETTLE);
    q.close();
    consumer.join();
    CHECK(!result);
}

static void close_wakes_blocked_push() {
    WQueue<int> q(1);
    q.push(1);
    bool result = true;
    std::thread producer([&] { result = q.push(2); });
    std::this_thread::sleep_for(SETTLE);
    q.close();
    producer.join();
    CHECK(!result);
}

// Items queued before close() still come out; only then does pop() say stop.
static void close_drains_before_stopping() {
    WQueue<int> q(4);
    q.push(1);
    q.push(2);
    q.close();

    CHECK(!q.push(3));

    int v = 0;
    CHECK(q.pop(v) && v == 1);
    CHECK(q.pop(v) && v == 2);
    CHECK(!q.pop(v));
}

// Several producers and consumers at once: nothing lost, nothing duplicated.
static void many_producers_many_consumers() {
    const int PRODUCERS = 4, CONSUMERS = 4, PER_PRODUCER = 20000;
    const int TOTAL = PRODUCERS * PER_PRODUCER;

    WQueue<int> q(64);
    std::vector<std::atomic<int>> seen(TOTAL);
    for (auto &s : seen) s = 0;

    std::vector<std::thread> consumers;
    for (int c = 0; c < CONSUMERS; c++)
        consumers.emplace_back([&] {
            int v;
            while (q.pop(v)) seen[v]++;
        });

    std::vector<std::thread> producers;
    for (int p = 0; p < PRODUCERS; p++)
        producers.emplace_back([&, p] {
            for (int i = 0; i < PER_PRODUCER; i++)
                q.push(p * PER_PRODUCER + i);
        });

    for (auto &t : producers) t.join();
    q.close();
    for (auto &t : consumers) t.join();

    int wrong = 0;
    for (auto &s : seen)
        if (s != 1) wrong++;
    CHECK(wrong == 0);
}

static void pool_runs_every_job_once() {
    const int JOBS = 10000;
    std::vector<std::atomic<int>> ran(JOBS);
    for (auto &r : ran) r = 0;

    Pool p(4, 128);
    for (int i = 0; i < JOBS; i++)
        CHECK(p.submit([&ran, i] { ran[i]++; }));
    p.stop();

    CHECK(p.completed() == JOBS);
    int wrong = 0;
    for (auto &r : ran)
        if (r != 1) wrong++;
    CHECK(wrong == 0);
}

// stop() right after submitting must still run the jobs already queued.
static void pool_stop_drains_queue() {
    std::atomic<int> ran(0);
    Pool p(2, 64);
    for (int i = 0; i < 50; i++)
        p.submit([&] {
            std::this_thread::sleep_for(1ms);
            ran++;
        });
    p.stop();
    CHECK(ran == 50);
    CHECK(p.completed() == 50);
}

static void pool_rejects_after_stop() {
    Pool p(2, 8);
    p.stop();
    p.stop();   // a second stop, and the destructor's, must be harmless
    CHECK(!p.submit([] {}));
}

int main() {
    struct { const char *name; void (*fn)(); } tests[] = {
        {"fifo_order", fifo_order},
        {"push_blocks_when_full", push_blocks_when_full},
        {"pop_blocks_when_empty", pop_blocks_when_empty},
        {"close_wakes_blocked_pop", close_wakes_blocked_pop},
        {"close_wakes_blocked_push", close_wakes_blocked_push},
        {"close_drains_before_stopping", close_drains_before_stopping},
        {"many_producers_many_consumers", many_producers_many_consumers},
        {"pool_runs_every_job_once", pool_runs_every_job_once},
        {"pool_stop_drains_queue", pool_stop_drains_queue},
        {"pool_rejects_after_stop", pool_rejects_after_stop},
    };

    for (auto &t : tests) {
        int before = failures;
        t.fn();
        printf("%s %s\n", failures == before ? "ok  " : "FAIL", t.name);
    }

    int n = sizeof(tests) / sizeof(tests[0]);
    printf("\n%d tests, %d failed checks\n", n, failures);
    return failures == 0 ? 0 : 1;
}
