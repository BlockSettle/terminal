#include <chrono>
#include <thread>
#include "FastLock.h"


FastLock::FastLock(std::atomic_flag &flag_to_lock)
    : flag(flag_to_lock)
{
    while (flag.test_and_set()) {
       std::this_thread::sleep_for(std::chrono::microseconds(1));
    }
}

FastLock::~FastLock()
{
    flag.clear();
}