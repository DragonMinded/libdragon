/*
    thread_utils: basic thread utilities
    Written by Giovanni Bajo <giovannibajo@gmail.com>

    This tool is part of the Libdragon SDK.

    This is free and unencumbered software released into the public domain.

    For more information, please refer to <http://unlicense.org/>
*/
#include <atomic>
#include <thread>
#include <functional>
#include <vector>
#include <algorithm>
#include <deque>
#include <mutex>
#include <condition_variable>

// Calls the function f in parallel using all requested threads, and then
// wait for all of them to finish.
inline void thParaLoop(std::function<void()> f, int threads_count=std::thread::hardware_concurrency()) {
    std::vector<std::thread> workers;
    for (int i=1; i<threads_count; i++) {
        workers.push_back(std::thread(f));
    }
    f();
    for (auto& t : workers) t.join();
}

// paraLoop(h, f) runs a sequence of "h" tasks using multiple tasks. The function
// will spawn the requested number of work thread, and call f(i) for each value
// in the range [0, h-1] using all available threads in parallel. 
inline void thParaLoop(int h, std::function<void(int)> f, int threads_count=std::thread::hardware_concurrency()) {
    std::atomic_int gy(0);
    thParaLoop([&](){
        for (int y=gy++; y<h; y=gy++) {
            f(y);
        }
    }, std::min(threads_count, h));
}

// A bounded queue to stream work items to consumer threads. push() blocks while
// the queue is full, providing backpressure to the producer; pop() blocks until
// an item is available, and returns false once the queue is closed and drained.
template <typename T> class thQueue {
    std::mutex mu;
    std::condition_variable cv;
    std::deque<T> q;
    size_t cap;
    bool closed = false;
public:
    thQueue(size_t cap) : cap(cap) {}

    void push(T v) {
        std::unique_lock<std::mutex> lock(mu);
        cv.wait(lock, [&]{ return q.size() < cap; });
        q.push_back(std::move(v));
        cv.notify_all();
    }

    bool pop(T &v) {
        std::unique_lock<std::mutex> lock(mu);
        cv.wait(lock, [&]{ return !q.empty() || closed; });
        if (q.empty()) return false;
        v = std::move(q.front()); q.pop_front();
        cv.notify_all();
        return true;
    }

    void close() {
        { std::lock_guard<std::mutex> lock(mu); closed = true; }
        cv.notify_all();
    }
};

// Like thParaLoop, but the tasks are pulled from a thQueue instead of being a
// fixed numeric range: each worker blocks waiting for an item, and then calls
// f(item, w) passing its own worker index. All the workers exit (and thus this
// function returns) once the queue is closed and fully drained.
template <typename T, typename F>
inline void thParaLoop(thQueue<T> &q, F f,
                       int threads_count=std::thread::hardware_concurrency()) {
    thParaLoop(threads_count, [&](int w) {
        T v;
        while (q.pop(v)) f(v, w);
    }, threads_count);
}
