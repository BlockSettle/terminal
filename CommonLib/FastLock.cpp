/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
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