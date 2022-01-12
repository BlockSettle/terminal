/*

***********************************************************************************
* Copyright (C) 2018 - 2021, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "UiUtils.h"

#include "ApplicationSettings.h"
#include "AuthAddressManager.h"
#include "BinaryData.h"
#include "BlockDataManagerConfig.h"
#include "BTCNumericTypes.h"
#include "BtcUtils.h"
#include "CoinControlModel.h"
#include "CustomControls/QtAwesome.h"
#include "SignContainer.h"
#include "TxClasses.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"

#include <QDateTime>
#include <QComboBox>
#include <QImage>
#include <QPixmap>
#include <QStyle>
#include <QAbstractItemModel>

#include <algorithm>

#include <qrencode.h>

// this symbol should be used as group separator, not ' ' ( space )
static const QChar defaultGroupSeparatorChar = QLatin1Char(0xa0);

template <typename T> bool contains(const std::vector<T>& v, const T& value)
{
   return std::find(v.begin(), v.end(), value) != v.end();
}

static const std::string XbtCurrencyString = "XBT";
const QString UiUtils::XbtCurrency = QString::fromStdString(XbtCurrencyString);

void UiUtils::SetupLocale()
{
   auto locale = QLocale(QLocale::C);
   auto options = locale.numberOptions();

   options |= QLocale::RejectGroupSeparator;
   options &= ~QLocale::OmitGroupSeparator;

   locale.setNumberOptions(options);

   QLocale::setDefault(locale);
}

QString UiUtils::displayDateTime(const QDateTime& datetime)
{
   static const QString format = QLatin1String("yyyy-MM-dd hh:mm:ss");
   return datetime.toString(format);
}

QString UiUtils::displayDateTime(uint32_t time) // in UnixTime
{
   return displayDateTime(QDateTime::fromTime_t(time));
}

QString UiUtils::displayTimeMs(const QDateTime& datetime)
{
   static const QString format = QLatin1String("hh:mm:ss.zzz");
   return datetime.toString(format);
}

constexpr int UiUtils::GetPricePrecisionXBT()
{
   return 2;
}

constexpr int UiUtils::GetPricePrecisionFX()
{
   return 4;
}

constexpr int UiUtils::GetPricePrecisionCC()
{
   return 6;
}


template<>
BTCNumericTypes::balance_type UiUtils::amountToBtc(BTCNumericTypes::balance_type value)
{
   return value;
}

namespace UiUtils {
   template <> QString displayAmount(double value)
   {
      if (std::isinf(value)) {
         return CommonUiUtilsText::tr("Loading...");
      }
      return UnifyValueString(QLocale().toString(value, 'f', GetAmountPrecisionXBT()));
   }

   template <> QString displayAmount(uint64_t value)
   {
      if (value == UINT64_MAX) {
         return CommonUiUtilsText::tr("Loading...");
      }
      return UnifyValueString(QLocale().toString(amountToBtc(value), 'f', GetAmountPrecisionXBT()));
   }

   template <> QString displayAmount(int64_t value)
   {
      if (value == INT64_MAX) {
         return CommonUiUtilsText::tr("Loading...");
      }
      return UnifyValueString(QLocale().toString(amountToBtc(value), 'f', GetAmountPrecisionXBT()));
   }

   QString displayAmount(const bs::XBTAmount &amount)
   {
      if (amount.isZero()) {
         return CommonUiUtilsText::tr("Loading...");
      }
      return UnifyValueString(QLocale().toString(amountToBtc(amount.GetValue())
         , 'f', GetAmountPrecisionXBT()));
   }

   double actualXbtPrice(bs::XBTAmount amount, double price)
   {
      int64_t ccAmountInCents = std::llround(amount.GetValueBitcoin() * price * 100);
      double ccAmount = ccAmountInCents / 100.;
      return  ccAmount / amount.GetValueBitcoin();
   }

   bs::hd::Purpose getHwWalletPurpose(WalletsTypes hwType)
   {
      if (!(hwType & WalletsTypes::HardwareAll)) {
         // incorrect function using
         assert(false);
         return {};
      }

      if (HardwareLegacy == hwType) {
         return bs::hd::Purpose::NonSegWit;
      }
      else if (HardwareNativeSW == hwType) {
         return bs::hd::Purpose::Native;
      }
      else if (HardwareNestedSW == hwType) {
         return bs::hd::Purpose::Nested;
      }

      // You should specify new case here
      assert(false);
      return {};
   }

   WalletsTypes getHwWalletType(bs::hd::Purpose purpose)
   {
      switch (purpose)
      {
      case bs::hd::Native:
         return WalletsTypes::HardwareNativeSW;
      case bs::hd::Nested:
         return WalletsTypes::HardwareNestedSW;
      case bs::hd::NonSegWit:
         return WalletsTypes::HardwareLegacy;
      default:
         return WalletsTypes::None;
      }
   }
}

int UiUtils::selectWalletInCombobox(QComboBox* comboBox, const std::string& walletId, WalletsTypes type /* = WalletsTypes::None */)
{
   int walletIndex = -1;
   if (comboBox->count() == 0) {
      return -1;
   }

   for (int i=0; i<comboBox->count(); ++i) {
      if (comboBox->itemData(i, WalletIdRole).toString().toStdString() == walletId) {
         if (type != WalletsTypes::None) {
            auto walletType =
               static_cast<UiUtils::WalletsTypes>(comboBox->itemData(i, WalletType).toInt());

            if ((type & walletType) == 0) {
               continue;
            }
         }

         walletIndex = i;
         break;
      }
   }

   if (comboBox->currentIndex() != walletIndex) {
      comboBox->setCurrentIndex(walletIndex);
   }
   return walletIndex;
}

int UiUtils::fillHDWalletsComboBox(QComboBox* comboBox, const std::shared_ptr<bs::sync::WalletsManager> &walletsManager
   , int walletTypes)
{
   if ((walletsManager == nullptr) || walletsManager->hdWallets().empty()) {
      return -1;
   }
   int selected = 0;
   const auto &priWallet = walletsManager->getPrimaryWallet();
   auto b = comboBox->blockSignals(true);
   comboBox->clear();

   auto addRow = [comboBox](const std::string& label, const std::string& walletId, WalletsTypes type) {
      if (WalletsTypes::None == type) {
         return;
      }

      int i = comboBox->count();
      comboBox->addItem(QString::fromStdString(label));
      comboBox->setItemData(i, QString::fromStdString(walletId), UiUtils::WalletIdRole);
      comboBox->setItemData(i, QVariant::fromValue(static_cast<int>(type)), UiUtils::WalletType);
   };

   for (const auto &hdWallet : walletsManager->hdWallets()) {
      if (hdWallet == priWallet) {
         selected = comboBox->count();
      }

      WalletsTypes type = WalletsTypes::None;
      // HW wallets marked as offline too, make sure to check that first
      if (!hdWallet->canMixLeaves()) {

         if (hdWallet->isHardwareOfflineWallet() && !(walletTypes & WalletsTypes::WatchOnly)) {
            continue;
         }

         for (auto const &leaf : hdWallet->getGroup(hdWallet->getXBTGroupType())->getLeaves()) {
            std::string label = hdWallet->name();
            type = WalletsTypes::None;

            auto purpose = leaf->purpose();
            if (purpose == bs::hd::Purpose::Native &&
               (walletTypes & WalletsTypes::HardwareNativeSW)) {
               label += " Native";
               type = WalletsTypes::HardwareNativeSW;
            }
            else if (purpose == bs::hd::Purpose::Nested &&
               (walletTypes & WalletsTypes::HardwareNestedSW)) {
               label += " Nested";
               type = WalletsTypes::HardwareNestedSW;
            }
            else if (purpose == bs::hd::Purpose::NonSegWit &&
               (walletTypes & WalletsTypes::HardwareLegacy) && leaf->getTotalBalance() > 0) {
               label += " Legacy";
               type = WalletsTypes::HardwareLegacy;
            }

            addRow(label, hdWallet->walletId(), type);
         }

         continue;
      }

      if (hdWallet->isOffline()) {
         type = (walletTypes & WalletsTypes::WatchOnly) ? WalletsTypes::WatchOnly : WalletsTypes::None;
      } else if (walletTypes & WalletsTypes::Full) {
         type = WalletsTypes::Full;
      }

      addRow(hdWallet->name(), hdWallet->walletId(), type);
   }
   comboBox->blockSignals(b);
   comboBox->setCurrentIndex(selected);
   return selected;
}

int UiUtils::fillHDWalletsComboBox(QComboBox* comboBox
   , const std::vector<bs::sync::HDWalletData>& wallets, int walletTypes)
{
   int selected = 0;
   const auto b = comboBox->blockSignals(true);
   comboBox->clear();

   auto addRow = [comboBox](const std::string& label, const std::string& walletId, WalletsTypes type)
   {
      if (WalletsTypes::None == type) {
         return;
      }
      int i = comboBox->count();
      comboBox->addItem(QString::fromStdString(label));
      comboBox->setItemData(i, QString::fromStdString(walletId), UiUtils::WalletIdRole);
      comboBox->setItemData(i, QVariant::fromValue(static_cast<int>(type)), UiUtils::WalletType);
   };

   for (const auto& hdWallet : wallets) {
      if (hdWallet.primary) {
         selected = comboBox->count();
      }

      WalletsTypes type = WalletsTypes::None;
      // HW wallets marked as offline too, make sure to check that first
/*      if (!hdWallet->canMixLeaves()) {

         if (hdWallet->isHardwareOfflineWallet() && !(walletTypes & WalletsTypes::WatchOnly)) {
            continue;
         }

         for (auto const& leaf : hdWallet->getGroup(hdWallet->getXBTGroupType())->getLeaves()) {
            std::string label = hdWallet->name();
            type = WalletsTypes::None;

            auto purpose = leaf->purpose();
            if (purpose == bs::hd::Purpose::Native &&
               (walletTypes & WalletsTypes::HardwareNativeSW)) {
               label += " Native";
               type = WalletsTypes::HardwareNativeSW;
            } else if (purpose == bs::hd::Purpose::Nested &&
               (walletTypes & WalletsTypes::HardwareNestedSW)) {
               label += " Nested";
               type = WalletsTypes::HardwareNestedSW;
            } else if (purpose == bs::hd::Purpose::NonSegWit &&
               (walletTypes & WalletsTypes::HardwareLegacy) && leaf->getTotalBalance() > 0) {
               label += " Legacy";
               type = WalletsTypes::HardwareLegacy;
            }
            addRow(label, hdWallet->walletId(), type);
         }
         continue;
      }*/   //TODO

      if (hdWallet.offline) {
         type = (walletTypes & WalletsTypes::WatchOnly) ? WalletsTypes::WatchOnly : WalletsTypes::None;
      } else if (walletTypes & WalletsTypes::Full) {
         type = WalletsTypes::Full;
      }

      addRow(hdWallet.name, hdWallet.id, type);
   }
   comboBox->blockSignals(b);
   comboBox->setCurrentIndex(selected);
   return selected;
}

void UiUtils::fillAuthAddressesComboBoxWithSubmitted(QComboBox* comboBox, const std::shared_ptr<AuthAddressManager> &authAddressManager)
{
   comboBox->clear();
   const auto &addrList = authAddressManager->GetSubmittedAddressList();
   if (!addrList.empty()) {
      const auto b = comboBox->blockSignals(true);
      for (const auto &address : addrList) {
         comboBox->addItem(QString::fromStdString(address.display()));
      }
      comboBox->blockSignals(b);
      QMetaObject::invokeMethod(comboBox, "setCurrentIndex", Q_ARG(int, authAddressManager->getDefaultIndex()));
      comboBox->setEnabled(true);
   } else {
      comboBox->setEnabled(false);
   }
}

void UiUtils::fillAuthAddressesComboBoxWithSubmitted(QComboBox* comboBox
   , const std::vector<bs::Address>& addrs, int defaultIdx)
{
   const auto b = comboBox->blockSignals(true);
   comboBox->clear();
   if (!addrs.empty()) {
      for (const auto& address : addrs) {
         comboBox->addItem(QString::fromStdString(address.display()));
      }
      comboBox->setEnabled(true);
      comboBox->blockSignals(b);
      comboBox->setCurrentIndex(defaultIdx);
   } else {
      comboBox->setEnabled(false);
   }
}

void UiUtils::fillRecvAddressesComboBox(QComboBox* comboBox, const std::shared_ptr<bs::sync::Wallet>& targetWallet)
{
   comboBox->clear();
   if (targetWallet) {
      comboBox->addItem(QObject::tr("Auto Create"));
      for (auto addr : targetWallet->getExtAddressList()) {
         comboBox->addItem(QString::fromStdString(addr.display()));
      }
      comboBox->setEnabled(true);
      comboBox->setCurrentIndex(0);
   }
   else {
      comboBox->setEnabled(false);
   }
}

void UiUtils::fillRecvAddressesComboBoxHDWallet(QComboBox* comboBox
   , const std::shared_ptr<bs::sync::hd::Wallet>& targetHDWallet, bool showRegularWalletsOnly)
{
   comboBox->clear();
   if (!targetHDWallet) {
      comboBox->setEnabled(false);
      return;
   }

   comboBox->addItem(QObject::tr("Auto Create"));
   for (const auto& wallet : targetHDWallet->getGroup(targetHDWallet->getXBTGroupType())->getLeaves()) {
      if (!showRegularWalletsOnly || wallet->purpose() != bs::hd::Purpose::NonSegWit) {
         for (auto addr : wallet->getExtAddressList()) {
            comboBox->addItem(QString::fromStdString(addr.display()));
         }
      }
   }

   comboBox->setEnabled(true);
   comboBox->setCurrentIndex(0);
}

void UiUtils::fillRecvAddressesComboBoxHDWallet(QComboBox* comboBox
   , const std::vector<bs::sync::WalletData>& wallets)
{
   comboBox->clear();
   comboBox->addItem(QObject::tr("Auto Create"));
   for (const auto& wd : wallets) {
      for (auto addr : wd.addresses) {
         comboBox->addItem(QString::fromStdString(addr.address.display()));
      }
   }
   comboBox->setEnabled(true);
   comboBox->setCurrentIndex(0);
}

QPixmap UiUtils::getQRCode(const QString& address, int size)
{
   QRcode* code = QRcode_encodeString(address.toLatin1(), 0, QR_ECLEVEL_L, QR_MODE_8, 1);
   if (code) {
      const int width = code->width;
      const int Margin = 1;
      const int imageWidth = 8 * (code->width + 2 * Margin);

      QImage image(imageWidth, imageWidth, QImage::Format_Mono);
      image.fill(1);

      for (int i = 0; i < width; ++i) {
         for (int l = 8*(i+Margin); l < 8*(i+Margin)+8; ++l) {
            for (int j = 0; j < width; ++j) {
               if (code->data[width * i + j] & 0x01) {
                  image.scanLine(l+Margin)[j+Margin] = 0x00;
               } else {
                  image.scanLine(l+Margin)[j+Margin] = 0xff;
               }
            }
         }
      }

      QRcode_free(code);
      if (size) {
         return QPixmap::fromImage(image.scaledToWidth(size));
      }
      else {
         return QPixmap::fromImage(image);
      }
   } else {
      return QPixmap();
   }
}

double UiUtils::parseAmountBtc(const QString& text, bool *converted)
{
   return QLocale().toDouble(text, converted);
}

QString UiUtils::displayCurrencyAmount(double amount)
{
   return UnifyValueString(QLocale().toString(amount, 'f', GetAmountPrecisionFX()));
}

QString UiUtils::displayCCAmount(double amount)
{
   return UnifyValueString(QLocale().toString(amount, 'f', GetAmountPrecisionCC()));
}

QString UiUtils::displayQuantity(double quantity, const QString& currency)
{
   return QStringLiteral("%2 %1").arg(UiUtils::displayQty(quantity, currency)).arg(currency);
}

QString UiUtils::displayQty(double quantity, const QString &currency)
{
   if (currency == XbtCurrency) {
      return UiUtils::displayAmount(quantity);
   }
   else {
      return UiUtils::displayCurrencyAmount(quantity);
   }
}

QString UiUtils::displayQuantity(double quantity, const std::string& currency)
{
   return displayQuantity(quantity, QString::fromStdString(currency));
}

QString UiUtils::displayQty(double quantity, const std::string &currency)
{
   return displayQty(quantity, QString::fromStdString(currency));
}

double UiUtils::truncatePriceForAsset(double price, bs::network::Asset::Type at)
{
   unsigned int multiplier = 0;

   switch(at) {
   case bs::network::Asset::SpotFX:
      multiplier = 10000;
      break;
   case bs::network::Asset::SpotXBT:
   case bs::network::Asset::DeliverableFutures:
      multiplier = 100;
      break;
   case bs::network::Asset::PrivateMarket:
      multiplier = 1000000;
      break;
   default:
      assert(false);
      return 0;
   }

   return (double)((int)(price*multiplier)) / multiplier;
}

QString UiUtils::displayPriceForAssetType(double price, bs::network::Asset::Type at)
{
   switch(at) {
   case bs::network::Asset::SpotFX:
      return UiUtils::displayPriceFX(price);
   case bs::network::Asset::SpotXBT:
   case bs::network::Asset::DeliverableFutures:
   case bs::network::Asset::CashSettledFutures:
      return UiUtils::displayPriceXBT(price);
   case bs::network::Asset::PrivateMarket:
      return UiUtils::displayPriceCC(price);
   default:
      assert(false);
      break;
   }

   return QString();
}

QString UiUtils::displayPriceFX(double price)
{
   return UnifyValueString(QLocale().toString(price, 'f', GetPricePrecisionFX()));
}

QString UiUtils::displayPriceXBT(double price)
{
   return UnifyValueString(QLocale().toString(price, 'f', GetPricePrecisionXBT()));
}

QString UiUtils::displayPriceCC(double price)
{
   return UnifyValueString(QLocale().toString(price, 'f', GetPricePrecisionCC()));
}

int UiUtils::GetPricePrecisionForAssetType(const bs::network::Asset::Type& assetType)
{
   switch(assetType) {
   case bs::network::Asset::SpotFX:
      return GetPricePrecisionFX();
   case bs::network::Asset::SpotXBT:
   case bs::network::Asset::DeliverableFutures:
   case bs::network::Asset::CashSettledFutures:
      return GetPricePrecisionXBT();
   case bs::network::Asset::PrivateMarket:
      return GetPricePrecisionCC();
   default:
      assert(false);
      break;
   }

   // Allow entering floating point numbers if the asset type was detected as Undefined
   return 6;
}

QString UiUtils::displayAmountForProduct(double quantity, const QString& product, bs::network::Asset::Type at)
{
   if (product == XbtCurrency) {
      return UiUtils::displayAmount(quantity);
   } else {
      if (at == bs::network::Asset::PrivateMarket) {
         return UiUtils::displayCCAmount(quantity);
      } else {
         return UiUtils::displayCurrencyAmount(quantity);
      }
   }
}

static void getPrecsFor(const std::string &security, const std::string &product, bs::network::Asset::Type at, int &qtyPrec, int &valuePrec)
{
   switch (at) {
   case bs::network::Asset::Type::SpotFX:
      qtyPrec = UiUtils::GetAmountPrecisionFX();
      valuePrec = UiUtils::GetAmountPrecisionFX();
      break;
   case bs::network::Asset::Type::SpotXBT:
   case bs::network::Asset::Type::DeliverableFutures:
   case bs::network::Asset::Type::CashSettledFutures:
      qtyPrec = UiUtils::GetAmountPrecisionXBT();
      valuePrec = UiUtils::GetAmountPrecisionFX();

      if (product != XbtCurrencyString) {
         std::swap(qtyPrec, valuePrec);
      }
      break;
   case bs::network::Asset::Type::PrivateMarket:
      qtyPrec = UiUtils::GetAmountPrecisionCC();
      // special case. display value for XBT with 6 decimals
      valuePrec = 6;
      break;
   default:
      assert(false);
      break;
   }
}

QString UiUtils::displayQty(double qty, const std::string &security, const std::string &product, bs::network::Asset::Type at)
{
   int qtyPrec = -1, valuePrec = -1;
   getPrecsFor(security, product, at, qtyPrec, valuePrec);
   return UnifyValueString(QLocale().toString(qty, 'f', qtyPrec));
}

QString UiUtils::displayValue(double value, const std::string &security, const std::string &product, bs::network::Asset::Type at)
{
   int qtyPrec = -1, valuePrec = -1;
   getPrecsFor(security, product, at, qtyPrec, valuePrec);
   return UnifyValueString(QLocale().toString(value, 'f', valuePrec));
}

QString UiUtils::displayAddress(const QString &addr)
{
   if (addr.length() <= 64) {
      return addr;
   }
   else {
      return addr.left(30) + QLatin1String("...") + addr.right(30);
   }
}

QString UiUtils::displayShortAddress(const QString &addr, const uint maxLength)
{
   if ((maxLength < 5) || ((uint)addr.length() <= maxLength)) {
      return addr;
   }

   const uint subLength = (maxLength - 3) / 2;
   return addr.left(subLength) + QLatin1String("...") + addr.right(subLength);
}


std::string UiUtils::getSelectedWalletId(QComboBox* comboBox)
{
   return comboBox->currentData(WalletIdRole).toString().toStdString();
}

UiUtils::WalletsTypes UiUtils::getSelectedWalletType(QComboBox* comboBox)
{
   return static_cast<UiUtils::WalletsTypes>(comboBox->currentData(WalletType).toInt());
}

bs::hd::Purpose UiUtils::getSelectedHwPurpose(QComboBox* comboBox)
{
   const auto walletType = static_cast<UiUtils::WalletsTypes>(
      comboBox->currentData(UiUtils::WalletType).toInt());
   return UiUtils::getHwWalletPurpose(walletType);
}

static QtAwesome* qtAwesome_ = nullptr;

void UiUtils::setupIconFont(QObject* parent)
{
   qtAwesome_ = new QtAwesome(parent);
   qtAwesome_->initInfinity();
}

QIcon UiUtils::icon(int character, const QVariantMap& options)
{
   return qtAwesome_->icon(character, options);
}

QIcon UiUtils::icon(const QString& name, const QVariantMap& options)
{
   return qtAwesome_->icon(name, options);
}

QIcon UiUtils::icon(const char* name, const QVariantMap& options)
{
   return qtAwesome_->icon(QLatin1String(name), options);
}

QString UiUtils::UnifyValueString(const QString& value)
{
   // we set C locale that use '.' as decimal separator. So no need to replace

   QString updatedValue = value;
   updatedValue.replace(QLocale().groupSeparator(), defaultGroupSeparatorChar);

   return updatedValue;
}

QString UiUtils::NormalizeString(const QString& value)
{
   QString copy{value};

   if (copy.startsWith(QLocale().decimalPoint())) {
      copy = QLatin1Char('0') + copy;
   }

   copy.remove(QLatin1Char(' '));
   copy.remove(defaultGroupSeparatorChar);

   return copy;
}


QValidator::State UiUtils::ValidateDoubleString(QString &input, int &pos, const int decimals)
{
   if (input.isEmpty()) {
      return QValidator::Acceptable;
   }

   static const QChar defaultDecimalsSeparatorChar = QLatin1Char('.');

   QString tempCopy = UiUtils::NormalizeString(input);

   QStringList list = tempCopy.split(defaultDecimalsSeparatorChar);
   if (list.size() == 2 && list[1].length() > decimals && pos == input.length()) {
      list[1].resize(decimals);
      input = list[0] + QLatin1Char('.') + list[1];
      tempCopy = input;
   }

   bool metDecimalSeparator = false;
   int afterDecimal = 0;

   const QChar zeroChar = QLatin1Char('0');

   bool decimalRequired = tempCopy.startsWith(zeroChar);

   for (int i=0; i < tempCopy.length(); i++) {
      const auto c = tempCopy.at(i);

      if (c.isDigit()) {
         if (decimalRequired) {
            if (c != zeroChar) {
               return QValidator::Invalid;
            }
         }
         if (metDecimalSeparator) {
            afterDecimal++;
            if (afterDecimal > decimals) {
               return QValidator::Invalid;
            }
         }
      } else if (c == defaultDecimalsSeparatorChar) {
         if (metDecimalSeparator || (decimals == 0)) {
            return QValidator::Invalid;
         }

         decimalRequired = false;
         metDecimalSeparator = true;
      } else {
         return QValidator::Invalid;
      }
   }

   bool converted = false;
   // don't need result, just check if could convert
   QLocale().toDouble(tempCopy, &converted);
   if (!converted) {
      return QValidator::Invalid;
   }

   return QValidator::Acceptable;
}

void UiUtils::setWrongState(QWidget *widget, bool wrong)
{
   widget->style()->unpolish(widget);
   widget->setProperty("wrongState", wrong);
   widget->style()->polish(widget);
}

ApplicationSettings::Setting UiUtils::limitRfqSetting(bs::network::Asset::Type type)
{
   switch (type) {
      case bs::network::Asset::SpotFX :
         return ApplicationSettings::FxRfqLimit;

      case bs::network::Asset::SpotXBT :
         return ApplicationSettings::XbtRfqLimit;

      case bs::network::Asset::PrivateMarket :
         return ApplicationSettings::PmRfqLimit;

      case bs::network::Asset::DeliverableFutures :
         return ApplicationSettings::FuturesLimit;

      default :
         assert(false);
         return ApplicationSettings::FxRfqLimit;
   }
}

ApplicationSettings::Setting UiUtils::limitRfqSetting(const QString &name)
{
   if (name == QString::fromUtf8(bs::network::Asset::toString(bs::network::Asset::SpotFX))) {
      return ApplicationSettings::FxRfqLimit;
   } else if (name == QString::fromUtf8(bs::network::Asset::toString(bs::network::Asset::SpotXBT))) {
      return ApplicationSettings::XbtRfqLimit;
   } else if (name ==
         QString::fromUtf8(bs::network::Asset::toString(bs::network::Asset::PrivateMarket))) {
            return ApplicationSettings::PmRfqLimit;
   } else if (name == QString::fromUtf8(bs::network::Asset::toString(bs::network::Asset::DeliverableFutures))) {
      return ApplicationSettings::FuturesLimit;
   } else {
      assert(false);
      return ApplicationSettings::FxRfqLimit;
   }
}

QString UiUtils::marketNameForLimit(ApplicationSettings::Setting s)
{
   switch (s) {
      case ApplicationSettings::FxRfqLimit :
         return QObject::tr(bs::network::Asset::toString(bs::network::Asset::SpotFX));

      case ApplicationSettings::XbtRfqLimit :
         return QObject::tr(bs::network::Asset::toString(bs::network::Asset::SpotXBT));

      case ApplicationSettings::PmRfqLimit :
         return QObject::tr(bs::network::Asset::toString(bs::network::Asset::PrivateMarket));

      case ApplicationSettings::FuturesLimit :
         return QObject::tr(bs::network::Asset::toString(bs::network::Asset::DeliverableFutures));

      default :
         assert(false);
         return QString();
   }
}

QString UiUtils::modelPath(const QModelIndex &index, QAbstractItemModel *model)
{
   if (model) {
      QModelIndex idx = model->index(index.row(), 0, index.parent());

      QString res = QString::fromLatin1("/") + idx.data().toString();

      while (idx.parent().isValid()) {
         idx = idx.parent();
         res.prepend(QString::fromLatin1("/") + idx.data().toString());
      }

      return res;
   } else {
      return QString();
   }
}


//
// WalletDescriptionValidator
//

UiUtils::WalletDescriptionValidator::WalletDescriptionValidator(QObject *parent) : QValidator(parent)
{}

QValidator::State UiUtils::WalletDescriptionValidator::validate(QString &input, int &pos) const
{
   static const QString invalidCharacters = QLatin1String("\\/?:*<>|");

   if (input.isEmpty()) {
      return QValidator::Acceptable;
   }

   if (invalidCharacters.contains(input.at(pos - 1))) {
      input.remove(pos - 1, 1);

      if (pos > input.size()) {
         --pos;
      }

      return QValidator::Invalid;
   }
   else {
      return QValidator::Acceptable;
   }
}
