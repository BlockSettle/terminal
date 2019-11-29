/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __HEADLESS_APP_H__
#define __HEADLESS_APP_H__

#include <atomic>
#include <functional>
#include <memory>

#include "SignerDefs.h"
#include "BSErrorCode.h"

namespace spdlog {
   class logger;
}
namespace bs {
   namespace core {
      class WalletsManager;
   }
}
namespace Blocksettle {
   namespace Communication {
      namespace signer {
         class Settings;
      }
   }
}
class HeadlessContainerListener;
class SignerAdapterListener;
class HeadlessSettings;
class ZmqBIP15XServerConnection;
class DispatchQueue;

class HeadlessAppObj
{
public:
   HeadlessAppObj(const std::shared_ptr<spdlog::logger> &
      , const std::shared_ptr<HeadlessSettings> &
      , const std::shared_ptr<DispatchQueue>&);

   ~HeadlessAppObj() noexcept;

   void start();
   void stop();

   void reloadWallets(const std::string &, const std::function<void()> &);
   void setLimits(bs::signer::Limits);
   void passwordReceived(const std::string &walletId, bs::error::ErrorCode result, const SecureBinaryData &);

   bs::error::ErrorCode activateAutoSign(const std::string &walletId, bool activate, const SecureBinaryData& password);

   void close();
   void walletsListUpdated();

   void updateSettings(const Blocksettle::Communication::signer::Settings&);

   ZmqBIP15XServerConnection* connection() const;
   bs::signer::BindStatus signerBindStatus() const { return signerBindStatus_; }
   BinaryData signerPubKey() const;

   static std::string getOwnKeyFileDir();
   static std::string getOwnKeyFileName();

private:
   void startTerminalsProcessing();
   void stopTerminalsProcessing();

private:
   std::shared_ptr<spdlog::logger>  logger_;
   const std::shared_ptr<HeadlessSettings>      settings_;
   const std::shared_ptr<DispatchQueue>         queue_;
   std::shared_ptr<bs::core::WalletsManager>    walletsMgr_;

   // Declare listeners before connections (they should be destroyed after)
   std::unique_ptr<HeadlessContainerListener>   terminalListener_;
   std::unique_ptr<SignerAdapterListener>       guiListener_;

   std::unique_ptr<ZmqBIP15XServerConnection>   terminalConnection_;
   std::unique_ptr<ZmqBIP15XServerConnection>   guiConnection_;

   std::atomic<bs::signer::BindStatus> signerBindStatus_{bs::signer::BindStatus::Inactive};
};

#endif // __HEADLESS_APP_H__
