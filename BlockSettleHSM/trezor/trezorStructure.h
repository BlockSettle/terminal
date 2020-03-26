/*

***********************************************************************************
* Copyright (C) 2016 - , BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef TREZORSTRUCTURE_H
#define TREZORSTRUCTURE_H

#include <QMetaType>
#include <functional>
#include <QString>
#include <string>
#include <QObject>
#include "CoreWallet.h"

using AsyncCallBack = std::function<void()>;
using AsyncCallBackCall = std::function<void(QVariant&&)>;

struct DeviceData
{
   QByteArray path_ = {};
   QByteArray vendor_ = {};
   QByteArray product_ = {};
   QByteArray sessionId_ = {};
   QByteArray debug_ = {};
   QByteArray debugSession_ = {};
};

enum class State {
   None = 0,
   Init,
   Enumerated,
   Acquired,
   Released
};

struct DeviceKey
{
   QString deviceLabel_;
   QString deviceId_;
   QString vendor_;
};

struct MessageData
{
   int msg_type_ = -1;
   int length_ = -1;
   std::string message_;
};

class HSMWalletWrapper {
   Q_GADGET
public:
   bs::core::wallet::HSMWalletInfo info_;
};
Q_DECLARE_METATYPE(HSMWalletWrapper)

struct HSMSignedTx {
   std::string signedTx;
};
Q_DECLARE_METATYPE(HSMSignedTx)

#endif // TREZORSTRUCTURE_H
