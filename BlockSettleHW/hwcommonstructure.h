/*

***********************************************************************************
* Copyright (C) 2016 - , BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef HWCOMMONSTRUCTURE_H
#define HWCOMMONSTRUCTURE_H

#include <QMetaType>
#include <functional>
#include <QString>
#include <string>
#include <QObject>
#include "CoreWallet.h"
#include "HDPath.h"

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

enum class DeviceType {
   None = 0,
   HWLedger,
   HWTrezor
};

struct DeviceKey
{
   QString deviceLabel_;
   QString deviceId_;
   QString vendor_;
   QString walletId_;
   QString status_;

   DeviceType type_ = DeviceType::None;
};

class HwWalletWrapper {
   Q_GADGET
public:
   bs::core::wallet::HwWalletInfo info_;
   Q_INVOKABLE QString walletName() {
      return QString::fromStdString(info_.label_);
   }
   Q_INVOKABLE QString walletDesc() {
      return QString::fromStdString(info_.vendor_);
   }
   bool isValid() {
      return !info_.xpubRoot_.empty() &&
         !info_.xpubNestedSegwit_.empty() &&
         !info_.xpubNativeSegwit_.empty();
   }
};
Q_DECLARE_METATYPE(HwWalletWrapper)

struct HWSignedTx {
   std::string signedTx;
};
Q_DECLARE_METATYPE(HWSignedTx)

bs::hd::Path getDerivationPath(bool testNet, bool isNestedSegwit);
#endif // HWCOMMONSTRUCTURE_H
