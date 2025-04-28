#pragma once

#include <atomic>
#include <mutex>

extern std::atomic<bool> stop_requested;
extern int thread_count;
extern int debug_level;
extern bool use_multithreading;
extern long long decimal_places;extern std::atomic<bool> stop_requested;
extern std::mutex console_mutex;

