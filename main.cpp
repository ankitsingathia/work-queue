#include "pool.h"
#include <cstdio>
#include <chrono>
#include <cmath>

const int JOBS = 200000;
const size_t CAP = 1024;

// Fake CPU work so the timings mean something.
static void burn() {
    volatile double x = 0;
    for (int i = 1; i < 400; i++) x += std::sqrt((double)i);
}

// Feeds JOBS jobs through a pool of n workers and returns ms elapsed.
static double run(int n) {
    auto t0 = std::chrono::steady_clock::now();

    Pool p(n, CAP);
    for (int i = 0; i < JOBS; i++) p.submit(burn);
    p.stop();

    auto t1 = std::chrono::steady_clock::now();
    std::chrono::duration<double, std::milli> ms = t1 - t0;

    if (p.completed() != JOBS)
        printf("  lost jobs: %d of %d\n", p.completed(), JOBS);

    return ms.count();
}

int main() {
    printf("hardware threads: %u\n", std::thread::hardware_concurrency());
    printf("jobs: %d, queue cap: %zu\n\n", JOBS, CAP);

    double base = 0;
    for (int n : {1, 2, 4, 8}) {
        double ms = run(n);
        if (n == 1) base = ms;
        printf("%d workers: %8.1f ms   %6.0f jobs/s   %.2fx\n",
               n, ms, JOBS / (ms / 1000.0), base / ms);
    }
    return 0;
}
