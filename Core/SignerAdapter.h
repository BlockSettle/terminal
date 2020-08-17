/*

***********************************************************************************
* Copyright (C) 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef SIGNER_ADAPTER_H
#define SIGNER_ADAPTER_H

#include <QObject>
#include "FutureValue.h"
#include "Message/Adapter.h"

namespace spdlog {
   class logger;
}
namespace BlockSettle {
   namespace Terminal {
      class SettingsMessage_SignerServer;
      class SignerMessage;
   }
}
class WalletSignerContainer;

class SignerAdapter : public QObject, public bs::message::Adapter
{
   Q_OBJECT
public:
   SignerAdapter(const std::shared_ptr<spdlog::logger> &);
   ~SignerAdapter() override = default;

   bool process(const bs::message::Envelope &) override;

   std::set<std::shared_ptr<bs::message::User>> supportedReceivers() const override {
      return { user_ };
   }
   std::string name() const override { return "Signer"; }

private:
   void start();

   bool processOwnRequest(const bs::message::Envelope &
      , const BlockSettle::Terminal::SignerMessage &);
   bool processSignerSettings(const BlockSettle::Terminal::SettingsMessage_SignerServer &);
   bool processNewKeyResponse(bool);
   std::shared_ptr<WalletSignerContainer> makeRemoteSigner(
      const BlockSettle::Terminal::SettingsMessage_SignerServer &);
   bool sendComponentLoading();
   void connectSignals();

private:
   std::shared_ptr<spdlog::logger>        logger_;
   std::shared_ptr<bs::message::User>     user_;
   std::shared_ptr<WalletSignerContainer> signer_;

   std::shared_ptr<FutureValue<bool>>  connFuture_;
   std::string    curServerId_;
   std::string    connKey_;
};


#endif	// SIGNER_ADAPTER_H
