#include "PubKeyLoader.h"
#include <QFile>
#include "BSMessageBox.h"


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
   default: break;
   }
   if (!keyString.empty()) {
      try {
         return BinaryData::CreateFromHex(keyString);
      }
      catch (...) {  // invalid key format
         return {};
      }
   }
   return loadKeyFromResource(kt, ApplicationSettings::EnvConfiguration(
      appSettings_->get<int>(ApplicationSettings::envConfiguration)));
}

BinaryData PubKeyLoader::loadKeyFromResource(KeyType kt, ApplicationSettings::EnvConfiguration ec)
{
   QString filename;
   switch (kt) {
   case KeyType::PublicBridge:
      filename = QStringLiteral("pub_");
      break;
   case KeyType::Chat:
      filename = QStringLiteral("chat_");
      break;
   default:    break;
   }
   switch (ec) {
   case ApplicationSettings::EnvConfiguration::PROD:
   case ApplicationSettings::EnvConfiguration::Custom:
      filename += QStringLiteral("prod");
      break;
   case ApplicationSettings::EnvConfiguration::UAT:
      filename += QStringLiteral("uat");
      break;
   case ApplicationSettings::EnvConfiguration::Staging:
      filename += QStringLiteral("staging");
      break;
   default:
      break;
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
   default:    return false;
   }
   return true;
}

ZmqBIP15XDataConnection::cbNewKey PubKeyLoader::getApprovingCallback(const KeyType kt
   , QWidget *parent, const std::shared_ptr<ApplicationSettings> &appSettings)
{
   // Define the callback that will be used to determine if the signer's BIP
   // 150 identity key, if it has changed, will be accepted. It needs strings
   // for the old and new keys, and a promise to set once the user decides.
   //
   // NB: This may need to be altered later. The PuB key should be hard-coded
   // and respected.
   return [kt, parent, appSettings] (const std::string& oldKey
         , const std::string& newKey, const std::string& srvAddrPort
         , const std::shared_ptr<std::promise<bool>> &newKeyProm) {
      QMetaObject::invokeMethod(parent, [kt, parent, appSettings, newKey, newKeyProm] {
         PubKeyLoader loader(appSettings);
         const auto newKeyBin = BinaryData::CreateFromHex(newKey);
         const auto oldKeyBin = loader.loadKey(kt);
         if (oldKeyBin == newKeyBin) {
            newKeyProm->set_value(true);
            return;
         }
         MessageBoxIdKey *box = new MessageBoxIdKey(BSMessageBox::question
            , QObject::tr("Server identity key has changed")
            , QObject::tr("Import %1 ID key?")
            .arg(serverName(kt))
            , QObject::tr("Old Key: %1\nNew Key: %2")
            .arg(oldKeyBin.isNull() ? QObject::tr("<none>") : QString::fromStdString(oldKeyBin.toHexStr()))
            .arg(QString::fromStdString(newKey))
            , parent);

         const bool answer = (box->exec() == QDialog::Accepted);
         box->deleteLater();
         if (answer) {
            loader.saveKey(kt, newKeyBin);
         }

         newKeyProm->set_value(answer);
      });
   };
}

QString PubKeyLoader::serverName(const KeyType kt)
{
   switch (kt) {
      case KeyType::PublicBridge:   return QObject::tr("PuB");
      case KeyType::Chat:           return QObject::tr("Chat");
      default:    break;
   }
   return {};
}
