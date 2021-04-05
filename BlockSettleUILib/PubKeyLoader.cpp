/*

***********************************************************************************
* Copyright (C) 2019 - 2021, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "PubKeyLoader.h"
#include <QFile>
#include <spdlog/fmt/fmt.h>
#include "BSMessageBox.h"
#include "ImportKeyBox.h"
#include "FutureValue.h"

PubKeyLoader::PubKeyLoader(const std::shared_ptr<ApplicationSettings> &appSettings
                           , const std::shared_ptr<BootstrapDataManager>& bootstrapDataManager)
   : appSettings_(appSettings)
   , bootstrapDataManager_{bootstrapDataManager}
{}

BinaryData PubKeyLoader::loadKey(const KeyType kt) const
{
   std::string keyString;
   switch (kt) {
   case KeyType::Chat:
      keyString = bootstrapDataManager_->getChatKey();
      break;
   case KeyType::Proxy:
      keyString = bootstrapDataManager_->getProxyKey();
      break;
   case KeyType::CcServer:
      keyString = bootstrapDataManager_->getCCTrackerKey();
      break;
   case KeyType::ExtConnector:
      keyString = appSettings_->get<std::string>(ApplicationSettings::ExtConnPubKey);
      break;
   }

   if (!keyString.empty()) {
      try {
         return BinaryData::CreateFromHex(keyString);
      }
      catch (...) {  // invalid key format
      }
   }

   return {};
}


bs::network::BIP15xNewKeyCb PubKeyLoader::getApprovingCallback(const KeyType kt
   , QWidget *bsMainWindow, const std::shared_ptr<ApplicationSettings> &appSettings
   , const std::shared_ptr<BootstrapDataManager>& bootstrapDataManager)
{
   // Define the callback that will be used to determine if the signer's BIP
   // 150 identity key, if it has changed, will be accepted. It needs strings
   // for the old and new keys, and a promise to set once the user decides.
   //
   // NB: This may need to be altered later. The PuB key should be hard-coded
   // and respected.
   return [kt, bsMainWindow, appSettings, bootstrapDataManager] (const std::string& oldKey
         , const std::string& newKeyHex, const std::string& srvAddrPort
         , const std::shared_ptr<FutureValue<bool>> &newKeyProm) {
      const auto &deferredDialog = [kt, bsMainWindow, appSettings, bootstrapDataManager, newKeyHex, newKeyProm, srvAddrPort]{
         QMetaObject::invokeMethod(bsMainWindow, [kt, bsMainWindow, appSettings, bootstrapDataManager, newKeyHex, newKeyProm, srvAddrPort] {
            PubKeyLoader loader(appSettings, bootstrapDataManager);
            const auto newKeyBin = BinaryData::CreateFromHex(newKeyHex);
            const auto oldKeyBin = loader.loadKey(kt);
            if (oldKeyBin == newKeyBin) {
               newKeyProm->setValue(true);
               return;
            }

            ImportKeyBox box (BSMessageBox::question
               , QObject::tr("Server is using a key that is different from the known BlockSettle signed key. This new key isn't signed. Accept %1 ID key?").arg(serverName(kt))
               , bsMainWindow);

            box.setNewKeyFromBinary(newKeyBin);
            box.setOldKeyFromBinary(oldKeyBin);
            box.setAddrPort(srvAddrPort);

            const bool answer = (box.exec() == QDialog::Accepted);

            newKeyProm->setValue(answer);
         });
      };

/*      BSTerminalMainWindow *mw = qobject_cast<BSTerminalMainWindow *>(bsMainWindow);
      if (mw) {
         mw->addDeferredDialog(deferredDialog);
      }*/
   };
}

std::string PubKeyLoader::envNameShort(ApplicationSettings::EnvConfiguration env)
{
   switch (env) {
      case ApplicationSettings::EnvConfiguration::Production:
         return "prod";
      case ApplicationSettings::EnvConfiguration::Test:
         return "test";
#ifndef PRODUCTION_BUILD
      case ApplicationSettings::EnvConfiguration::Staging:
            return "staging";
      case ApplicationSettings::EnvConfiguration::Custom:
         return "custom";
#endif
   }
   return "unknown";
}

std::string PubKeyLoader::serverNameShort(PubKeyLoader::KeyType kt)
{
   switch (kt) {
      case KeyType::Chat:           return "chat";
      case KeyType::Proxy:          return "proxy";
      case KeyType::CcServer:       return "cctracker";
      case KeyType::MdServer:       return "mdserver";
      case KeyType::Mdhs:           return "mdhs";
   }
   return {};
}

std::string PubKeyLoader::serverHostName(PubKeyLoader::KeyType kt, ApplicationSettings::EnvConfiguration env)
{
   return fmt::format("{}-{}.blocksettle.com", serverNameShort(kt), envNameShort(env));
}

std::string PubKeyLoader::serverHttpPort()
{
   return "80";
}

std::string PubKeyLoader::serverHttpsPort()
{
   return "443";
}

QString PubKeyLoader::serverName(const KeyType kt)
{
   switch (kt) {
      case KeyType::Chat:           return QObject::tr("Chat Server");
      case KeyType::Proxy:          return QObject::tr("Proxy");
      case KeyType::CcServer:       return QObject::tr("CC tracker server");
      case KeyType::ExtConnector:   return QObject::tr("External Connector");
   }
   return {};
}
