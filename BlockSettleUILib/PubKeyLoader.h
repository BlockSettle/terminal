#ifndef PUB_KEY_LOADER_H
#define PUB_KEY_LOADER_H

#include "ApplicationSettings.h"
#include "BinaryData.h"
#include "ZMQ_BIP15X_Helpers.h"

class QWidget;

class PubKeyLoader
{
public:
   PubKeyLoader(const std::shared_ptr<ApplicationSettings> &);
   ~PubKeyLoader() noexcept = default;

   PubKeyLoader(const PubKeyLoader&) = delete;
   PubKeyLoader& operator = (const PubKeyLoader&) = delete;
   PubKeyLoader(PubKeyLoader&&) = delete;
   PubKeyLoader& operator = (PubKeyLoader&&) = delete;

   enum class KeyType {
      Undefined,
      PublicBridge,
      Chat
   };

   BinaryData loadKey(const KeyType) const;
   bool saveKey(const KeyType, const BinaryData &);

   static ZmqBipNewKeyCb getApprovingCallback(const KeyType
      , QWidget *bsMainWindow, const std::shared_ptr<ApplicationSettings> &);

private:
   static BinaryData loadKeyFromResource(KeyType, ApplicationSettings::EnvConfiguration);
   static QString serverName(const KeyType);

private:
   std::shared_ptr<ApplicationSettings>   appSettings_;
};

#endif // PUB_KEY_LOADER_H
