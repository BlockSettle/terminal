/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef TERMINAL_MESSAGE_H
#define TERMINAL_MESSAGE_H

#include "Message/Bus.h"
#include "Message/Envelope.h"

namespace bs {

   class MainLoopRuner
   {
   public:
      virtual void run() = 0;
   };

   namespace message {
      enum class TerminalUsers : UserValue
      {
         Unknown = 0,
         BROADCAST,
         Signer,
         API,
         Settings,
         BsServer,      // General target/origin of all sorts of BS server messages
         Matching,      // Alias for Celer or other matching system
         Assets,        // Alias for Genoa data storage atm
         MktData,
         MDHistory,     // Charts data storage
         Blockchain,    // General name for Armory connection
         Wallets,
         Settlement,
         Chat,
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
      };

/*      Envelope pbEnvelope(uint64_t id, TerminalUsers sender, TerminalUsers receiver
         , const TimeStamp &posted, const TimeStamp &execAt, const std::string &message
         , bool request = false);
      Envelope pbEnvelope(uint64_t id, UsersPB sender, const std::shared_ptr<User> &receiver
         , const std::string &message);
      Envelope pbEnvelope(UsersPB sender, UsersPB receiver, const std::string &message
         , bool request = false, const TimeStamp &execAt = {});
      Envelope pbEnvelope(const std::shared_ptr<User> &sender, UsersPB receiver
         , const std::string &message, bool request = false, const TimeStamp &execAt = {});
*/

      class TerminalInprocBus : public Bus
      {
      public:
         TerminalInprocBus(const std::shared_ptr<spdlog::logger> &);
         ~TerminalInprocBus() override;

         void addAdapter(const std::shared_ptr<Adapter> &) override;

         void shutdown();
         bool run();

      private:
         std::shared_ptr<spdlog::logger>  logger_;
         std::shared_ptr<Queue>  queue_;
         std::shared_ptr<MainLoopRuner>   runnableAdapter_;
      };

   } // namespace message
} // namespace bs

#endif	// TERMINAL_MESSAGE_H
