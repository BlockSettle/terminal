/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef CHAT_SETTINGS_H
#define CHAT_SETTINGS_H

#include <memory>
#include <QMetaType>
#include <QString>
#include "BinaryData.h"
#include "SecureBinaryData.h"

class ConnectionManager;
using ConnectionManagerPtr = std::shared_ptr<ConnectionManager>;

namespace Chat {

   struct ChatSettings
   {
      ConnectionManagerPtr connectionManager;
      SecureBinaryData chatPrivKey;
      BinaryData chatPubKey;
      std::string chatServerHost;
      std::string chatServerPort;
      QString chatDbFile;
   };

}

Q_DECLARE_METATYPE(Chat::ChatSettings)

#endif
