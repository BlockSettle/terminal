/*

***********************************************************************************
* Copyright (C) 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef TERMINAL_MESSAGE_H
#define TERMINAL_MESSAGE_H

#include "Message/Bus.h"
#include "Message/Envelope.h"

namespace BlockSettle {
   namespace Terminal {
      class MatchingMessage_Quote;
      class MatchingMessage_Order;
   }
}

namespace bs {

   class MainLoopRuner
   {
   public:
      virtual void run(int &argc, char **argv) = 0;
   };

   namespace message {
      enum class TerminalUsers : UserValue
      {
         Unknown = 0,
         BROADCAST,
         System,        // Used only as a sender of administrative messages, no adapter registers it
         Signer,        // TX signing and core wallets loading
         API,           // API adapter and bus for API clients
         Settings,      // All kinds of static data
         BsServer,      // General target/origin of all sorts of BS server messages
         Matching,      // Alias for Celer (or other matching system in the future)
         Assets,        // Assets manipulation
         MktData,       // Market data retranslation
         MDHistory,     // Charts data storage
         Blockchain,    // General name for Armory connection
         Wallets,       // Sync wallet routines (loading, balance, UTXO reservation)
         OnChainTracker,// Auth & CC tracker combined in one adapter
         Settlement,    // All settlements (FX, XBT, CC) for both dealer and requester
         Chat,          // Chat network routines
         UsersCount
      };

      class UserTerminal : public User
      {
      public:
         UserTerminal(TerminalUsers value) : User(static_cast<UserValue>(value)) {}

         std::string name() const override;

         bool isBroadcast() const override
         {
            return (static_cast<TerminalUsers>(value()) == TerminalUsers::BROADCAST);
         }

         bool isFallback() const override
         {
            return (static_cast<TerminalUsers>(value()) == TerminalUsers::Unknown);
         }

         bool isSystem() const override
         {
            return (static_cast<TerminalUsers>(value()) == TerminalUsers::System);
         }

         static std::shared_ptr<UserTerminal> create(TerminalUsers value)
         {
            return std::make_shared<UserTerminal>(value);
         }
      };

      class TerminalInprocBus : public Bus
      {
      public:
         TerminalInprocBus(const std::shared_ptr<spdlog::logger> &);
         ~TerminalInprocBus() override;

         void addAdapter(const std::shared_ptr<Adapter> &) override;

         void shutdown();
         bool run(int &argc, char **argv);

      private:
         void start();

      private:
         std::shared_ptr<spdlog::logger>  logger_;
         std::shared_ptr<Queue>  queue_;
         std::shared_ptr<MainLoopRuner>   runnableAdapter_;
      };

   } // namespace message
} // namespace bs

#endif	// TERMINAL_MESSAGE_H
