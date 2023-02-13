/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
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

#include "Wallets/SignerDefs.h"
#include "BSErrorCode.h"

namespace spdlog {
   class logger;
}
namespace bs {
   namespace core {
      class WalletsManager;
   }
   namespace network {
      struct BIP15xServerParams;
      class TransportBIP15xServer;
   }
}
namespace Blocksettle {
   namespace Communication {
      namespace signer {
         class Settings;
         enum ControlPasswordStatus : int;
      }
   }
}
class Bip15xServerConnection;
class DispatchQueue;
class HeadlessContainerListener;
class HeadlessSettings;
class ServerConnection;
class SignerAdapterListener;
class AuthorizedPeers;

class HeadlessAppObj
{
public:
   HeadlessAppObj(const std::shared_ptr<spdlog::logger> &
      , const std::shared_ptr<HeadlessSettings> &
      , const std::shared_ptr<DispatchQueue>&);

   ~HeadlessAppObj() noexcept;

   void start();
   void stop();

   void reloadWallets(bool notifyGUI, const std::function<void()> & = nullptr);
   void setLimits(bs::signer::Limits);
   void passwordReceived(const std::string &walletId, bs::error::ErrorCode result, const SecureBinaryData &);

   bs::error::ErrorCode activateAutoSign(const std::string &walletId, bool activate, const SecureBinaryData& password);

   void close();
   void walletsListUpdated();

   void updateSettings(const Blocksettle::Communication::signer::Settings&);

   ServerConnection* connection() const;
   bs::signer::BindStatus signerBindStatus() const { return signerBindStatus_; }
   BinaryData signerPubKey() const;

   static std::string getOwnKeyFileDir();
   static std::string getOwnKeyFileName();

   SecureBinaryData controlPassword() const;
   void setControlPassword(const SecureBinaryData &controlPassword);
   bs::error::ErrorCode changeControlPassword(const SecureBinaryData &controlPasswordOld, const SecureBinaryData &controlPasswordNew);

   bs::network::BIP15xServerParams getGuiServerParams(void) const;

private:
   void startTerminalsProcessing();
   void stopTerminalsProcessing();
   void applyNewControlPassword(const SecureBinaryData &controlPassword, bool notifyGui);

private:
   std::shared_ptr<spdlog::logger>  logger_;
   const std::shared_ptr<HeadlessSettings>      settings_;
   const std::shared_ptr<DispatchQueue>         queue_;
   std::shared_ptr<bs::core::WalletsManager>    walletsMgr_;

   // Declare listeners before connections (they should be destroyed after)
   std::unique_ptr<HeadlessContainerListener>   terminalListener_;
   std::unique_ptr<SignerAdapterListener>       guiListener_;

   std::unique_ptr<ServerConnection>   terminalConnection_;
   std::shared_ptr<bs::network::TransportBIP15xServer>   terminalTransport_;
   std::shared_ptr<ServerConnection>      guiConnection_;
   std::shared_ptr<bs::network::TransportBIP15xServer>   guiTransport_;

   std::atomic<bs::signer::BindStatus> signerBindStatus_{bs::signer::BindStatus::Inactive};

   SecureBinaryData controlPassword_;
   Blocksettle::Communication::signer::ControlPasswordStatus controlPasswordStatus_;
   BinaryData signerPubKey_;

};

#endif // __HEADLESS_APP_H__
