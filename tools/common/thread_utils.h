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
