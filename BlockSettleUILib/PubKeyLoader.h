/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef PUB_KEY_LOADER_H
#define PUB_KEY_LOADER_H

#include "ApplicationSettings.h"
#include "BinaryData.h"
#include "BIP15xHelpers.h"
#include "BootstrapDataManager.h"

class QWidget;

class PubKeyLoader
{
public:
   PubKeyLoader(const std::shared_ptr<ApplicationSettings> &
                , const std::shared_ptr<BootstrapDataManager>& bootstrapDataManager);
   ~PubKeyLoader() noexcept = default;

   PubKeyLoader(const PubKeyLoader&) = delete;
   PubKeyLoader& operator = (const PubKeyLoader&) = delete;
   PubKeyLoader(PubKeyLoader&&) = delete;
   PubKeyLoader& operator = (PubKeyLoader&&) = delete;

   enum class KeyType {
      Chat = 2,
      Proxy,
      CcServer,
      MdServer,
      Mdhs,
      ExtConnector
   };

   BinaryData loadKey(const KeyType) const;

   static bs::network::BIP15xNewKeyCb getApprovingCallback(const KeyType
      , QWidget *bsMainWindow, const std::shared_ptr<ApplicationSettings> &
      , const std::shared_ptr<BootstrapDataManager>& bootstrapDataManager);

   static std::string envNameShort(ApplicationSettings::EnvConfiguration env);
   static std::string serverNameShort(KeyType);
   static std::string serverHostName(KeyType, ApplicationSettings::EnvConfiguration env);
   static std::string serverHttpPort();
   static std::string serverHttpsPort();

private:
   static QString serverName(const KeyType);

private:
   std::shared_ptr<ApplicationSettings>   appSettings_;
   std::shared_ptr<BootstrapDataManager>  bootstrapDataManager_;
};

#endif // PUB_KEY_LOADER_H
