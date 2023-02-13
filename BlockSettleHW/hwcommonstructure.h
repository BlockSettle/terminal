/*

***********************************************************************************
* Copyright (C) 2020 - 2021, BlockSettle AB
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

// There is no way to determinate difference between ledger devices
// so we use vendor name for identification
const std::string kDeviceLedgerId = "Ledger";

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
      return QString::fromStdString(info_.label);
   }
   Q_INVOKABLE QString walletDesc() {
      return QString::fromStdString(info_.vendor);
   }
   bool isValid() {
      return !info_.xpubRoot.empty() &&
         !info_.xpubNestedSegwit.empty() &&
         !info_.xpubNativeSegwit.empty() &&
         !info_.xpubLegacy.empty();
   }

   bool isFirmwareSupported_{true};
   std::string firmwareSupportedMsg_;
};
Q_DECLARE_METATYPE(HwWalletWrapper)

struct HWSignedTx {
   std::string signedTx;
};
Q_DECLARE_METATYPE(HWSignedTx)

bs::hd::Path getDerivationPath(bool testNet, bs::hd::Purpose element);
bool isNestedSegwit(const bs::hd::Path& path);
bool isNativeSegwit(const bs::hd::Path& path);
bool isNonSegwit(const bs::hd::Path& path);

namespace HWInfoStatus {
   const QString kPressButton = QObject::tr("Confirm transaction output(s) on your device");
   const QString kTransaction = QObject::tr("Loading transaction to your device....");
   const QString kReceiveSignedTx = QObject::tr("Receiving signed transaction from device....");
   const QString kTransactionFinished = QObject::tr("Transaction signing finished with success");
   const QString kCancelledByUser = QObject::tr("Cancelled by user");
}

#endif // HWCOMMONSTRUCTURE_H
