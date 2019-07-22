#ifndef SIGNALS_HANDLER_H
#define SIGNALS_HANDLER_H

#include <csignal>
#include <functional>

class SignalsHandler
{
public:
   using Cb = std::function<void(int signal)>;

   // Starts background thread that will wait for the listed POSIX signals.
   // Must be called from main thread before starting other threads (we use sigwait and need to block listed signals first).
   // Does nothing on Windows.
   static void registerHandler(const Cb &cb, const std::initializer_list<int> &signalsList = { SIGTERM, SIGINT });
};

#endif
