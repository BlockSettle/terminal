/*

***********************************************************************************
* Copyright (C) 2016 - , BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "PubKeyLoader.h"
#include <QFile>
#include "BSMessageBox.h"
#include "ImportKeyBox.h"
#include "BSTerminalMainWindow.h"
#include "FutureValue.h"

PubKeyLoader::PubKeyLoader(const std::shared_ptr<ApplicationSettings> &appSettings)
   : appSettings_(appSettings)
{}

BinaryData PubKeyLoader::loadKey(const KeyType kt) const
{
   std::string keyString;
   switch (kt) {
   case KeyType::PublicBridge:
      keyString = appSettings_->get<std::string>(ApplicationSettings::pubBridgePubKey);
      break;
   case KeyType::Chat:
      keyString = appSettings_->get<std::string>(ApplicationSettings::chatServerPubKey);
      break;
   case KeyType::Proxy:
      keyString = appSettings_->get<std::string>(ApplicationSettings::proxyServerPubKey);
      break;
   case KeyType::CcServer:
      keyString = appSettings_->get<std::string>(ApplicationSettings::ccServerPubKey);
      break;
   }

   if (!keyString.empty()) {
      try {
         return BinaryData::CreateFromHex(keyString);
      }
      catch (...) {  // invalid key format
         return {};
      }
   }
   return loadKeyFromResource(kt, appSettings_->getEnvConf(), appSettings_->getNetType());
}

BinaryData PubKeyLoader::loadKeyFromResource(KeyType kt, ApplicationSettings::EnvConfiguration ec, NetworkType type)
{
   QString filename;
   switch (kt) {
   case KeyType::PublicBridge:
      filename = QStringLiteral("pub_");
      break;
   case KeyType::Chat:
      filename = QStringLiteral("chat_");
      break;
   case KeyType::Proxy:
      filename = QStringLiteral("proxy_");
      break;
   case KeyType::CcServer:
      filename = QStringLiteral("cc_server_");
      break;
   }
   assert(!filename.isEmpty());

   switch (ec) {
   case ApplicationSettings::EnvConfiguration::ProductionAndUat:
      if (type == NetworkType::MainNet) {
         filename += QStringLiteral("prod");
      } else {
         filename += QStringLiteral("uat");
      }
      break;
#ifndef PRODUCTION_BUILD
   case ApplicationSettings::EnvConfiguration::Staging:
      filename += QStringLiteral("staging");
      break;
   case ApplicationSettings::EnvConfiguration::Custom:
      filename += QStringLiteral("prod");
      break;
#endif
   }
   if (filename.isEmpty()) {
      return {};
   }
   else {
      filename = QStringLiteral(":/resources/PublicKeys/") + filename
         + QStringLiteral(".key");
   }
   QFile f(filename);
   if (!f.open(QIODevice::ReadOnly)) {
      return {};
   }
   BinaryData result;
   try {
      result.createFromHex(f.readAll().toStdString());
   }
   catch (...) {
      return {};
   }
   return result;
}

bool PubKeyLoader::saveKey(const KeyType kt, const BinaryData &key)
{
   switch (kt) {
   case KeyType::PublicBridge:
      appSettings_->set(ApplicationSettings::pubBridgePubKey, QString::fromStdString(key.toHexStr()));
      break;
   case KeyType::Chat:
      appSettings_->set(ApplicationSettings::chatServerPubKey, QString::fromStdString(key.toHexStr()));
      break;
   case KeyType::Proxy:
      appSettings_->set(ApplicationSettings::proxyServerPubKey, QString::fromStdString(key.toHexStr()));
      break;
   case KeyType::CcServer:
      appSettings_->set(ApplicationSettings::ccServerPubKey, QString::fromStdString(key.toHexStr()));
      break;
   }
   return true;
}

ZmqBipNewKeyCb PubKeyLoader::getApprovingCallback(const KeyType kt
   , QWidget *bsMainWindow, const std::shared_ptr<ApplicationSettings> &appSettings)
{
   // Define the callback that will be used to determine if the signer's BIP
   // 150 identity key, if it has changed, will be accepted. It needs strings
   // for the old and new keys, and a promise to set once the user decides.
   //
   // NB: This may need to be altered later. The PuB key should be hard-coded
   // and respected.
   return [kt, bsMainWindow, appSettings] (const std::string& oldKey
         , const std::string& newKeyHex, const std::string& srvAddrPort
         , const std::shared_ptr<FutureValue<bool>> &newKeyProm) {
      const auto &deferredDialog = [kt, bsMainWindow, appSettings, newKeyHex, newKeyProm, srvAddrPort]{
         QMetaObject::invokeMethod(bsMainWindow, [kt, bsMainWindow, appSettings, newKeyHex, newKeyProm, srvAddrPort] {
            PubKeyLoader loader(appSettings);
            const auto newKeyBin = BinaryData::CreateFromHex(newKeyHex);
            const auto oldKeyBin = loader.loadKey(kt);
            if (oldKeyBin == newKeyBin) {
               newKeyProm->setValue(true);
               return;
            }

            ImportKeyBox box (BSMessageBox::question
               , QObject::tr("Import %1 ID key?").arg(serverName(kt))
               , bsMainWindow);

            box.setNewKeyFromBinary(newKeyBin);
            box.setOldKeyFromBinary(oldKeyBin);
            box.setAddrPort(srvAddrPort);

            const bool answer = (box.exec() == QDialog::Accepted);
            if (answer) {
               loader.saveKey(kt, newKeyBin);
            }

            newKeyProm->setValue(answer);
         });
      };

      BSTerminalMainWindow *mw = qobject_cast<BSTerminalMainWindow *>(bsMainWindow);
      if (mw) {
         mw->addDeferredDialog(deferredDialog);
      }
   };
}

QString PubKeyLoader::serverName(const KeyType kt)
{
   switch (kt) {
      case KeyType::PublicBridge:   return QObject::tr("PuB");
      case KeyType::Chat:           return QObject::tr("Chat Server");
      case KeyType::Proxy:          return QObject::tr("Proxy");
      case KeyType::CcServer:       return QObject::tr("CC tracker server");
   }
   return {};
}
