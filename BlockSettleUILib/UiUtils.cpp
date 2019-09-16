#include "UiUtils.h"

#include "ApplicationSettings.h"
#include "AuthAddressManager.h"
#include "BinaryData.h"
#include "BlockDataManagerConfig.h"
#include "BTCNumericTypes.h"
#include "BtcUtils.h"
#include "CoinControlModel.h"
#include "QtAwesome.h"
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

const QLatin1String UiUtils::XbtCurrency = QLatin1String("XBT");

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

QString UiUtils::displayDateTime(uint64_t time)
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
}

int UiUtils::fillWalletsComboBox(QComboBox* comboBox, const std::shared_ptr<bs::sync::WalletsManager> &walletsManager
   , bool skipWatchingOnly)
{
   auto addHdWallet = [comboBox, skipWatchingOnly](const std::shared_ptr<bs::sync::hd::Wallet> &hdWallet) {
      if (skipWatchingOnly && hdWallet->isOffline()) {
         return;
      }

      for (const auto &group : hdWallet->getGroups()) {
         if (group->type() != bs::core::wallet::Type::Bitcoin) {
            continue;
         }

         const auto prefix = QString::fromStdString(hdWallet->name())
            + QLatin1String("/") + QString::fromStdString(group->name()) + QLatin1String("/");

         for (const auto &leaf : group->getLeaves()) {
            const int index = comboBox->count();
            comboBox->addItem(prefix + QString::fromStdString(leaf->shortName()));
            comboBox->setItemData(index, QString::fromStdString(leaf->walletId()), UiUtils::WalletIdRole);
            comboBox->setItemData(index, leaf->getSpendableBalance(), UiUtils::WalletBalanceRole);
         }
      }
   };

   auto b = comboBox->blockSignals(true);
   comboBox->clear();
   auto primaryWallet = walletsManager->getPrimaryWallet();
   if (primaryWallet) {
      // Let's add primary HD wallet first if exists
      addHdWallet(primaryWallet);
   }
   for (int i = 0; i < int(walletsManager->hdWalletsCount()); ++i) {
      const auto &hdWallet = walletsManager->getHDWallet(unsigned(i));
      if (hdWallet != primaryWallet) {
         addHdWallet(hdWallet);
      }
   }
   comboBox->blockSignals(b);

   if (comboBox->count() == 0) {
      return -1;
   }
   comboBox->setCurrentIndex(0);
   return 0;
}

int UiUtils::selectWalletInCombobox(QComboBox* comboBox, const std::string& walletId)
{
   int walletIndex = -1;
   if (comboBox->count() == 0) {
      return -1;
   }

   for (int i=0; i<comboBox->count(); ++i) {
      if (comboBox->itemData(i, WalletIdRole).toString().toStdString() == walletId) {
         walletIndex = i;
         break;
      }
   }

   if (comboBox->currentIndex() != walletIndex) {
      comboBox->setCurrentIndex(walletIndex);
   }
   return walletIndex;
}

int UiUtils::fillHDWalletsComboBox(QComboBox* comboBox, const std::shared_ptr<bs::sync::WalletsManager> &walletsManager)
{
   if ((walletsManager == nullptr) || (walletsManager->hdWalletsCount() == 0)) {
      return -1;
   }
   int selected = 0;
   const auto &priWallet = walletsManager->getPrimaryWallet();
   auto b = comboBox->blockSignals(true);
   comboBox->clear();
   for (size_t i = 0; i < walletsManager->hdWalletsCount(); i++) {
      const auto &hdWallet = walletsManager->getHDWallet(i);
      if (hdWallet == priWallet) {
         selected = i;
      }
      comboBox->addItem(QString::fromStdString(hdWallet->name()));
      comboBox->setItemData(i, QString::fromStdString(hdWallet->walletId()), UiUtils::WalletIdRole);
   }
   comboBox->blockSignals(b);
   QMetaObject::invokeMethod(comboBox, "setCurrentIndex", Q_ARG(int, selected));
   return selected;
}

void UiUtils::fillAuthAddressesComboBox(QComboBox* comboBox, const std::shared_ptr<AuthAddressManager> &authAddressManager)
{
   comboBox->clear();
   const auto &addrList = authAddressManager->GetVerifiedAddressList();
   if (!addrList.empty()) {
      const auto b = comboBox->blockSignals(true);
      for (const auto &address : addrList) {
         comboBox->addItem(QString::fromStdString(address.display()));
      }
      comboBox->blockSignals(b);
      QMetaObject::invokeMethod(comboBox, "setCurrentIndex", Q_ARG(int, authAddressManager->getDefaultIndex()));
      comboBox->setEnabled(true);
   }
   else {
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

void UiUtils::fillRecvAddressesComboBoxHDWallet(QComboBox* comboBox, const std::shared_ptr<bs::sync::hd::Wallet>& targetHDWallet)
{
   comboBox->clear();
   if (!targetHDWallet) {
      comboBox->setEnabled(false);
      return;
   }

   comboBox->addItem(QObject::tr("Auto Create"));
   for (const auto& wallet : targetHDWallet->getGroup(targetHDWallet->getXBTGroupType())->getAllLeaves()) {
      for (auto addr : wallet->getExtAddressList()) {
         comboBox->addItem(QString::fromStdString(addr.display()));
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

double UiUtils::parseAmountBtc(const QString& text)
{
   return QLocale().toDouble(text);
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
      multiplier = 100;
      break;
   case bs::network::Asset::PrivateMarket:
      multiplier = 1000000;
      break;
   default:
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
      return UiUtils::displayPriceXBT(price);
   case bs::network::Asset::PrivateMarket:
      return UiUtils::displayPriceCC(price);
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
      return GetPricePrecisionXBT();
   case bs::network::Asset::PrivateMarket:
      return GetPricePrecisionCC();
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
      qtyPrec = UiUtils::GetAmountPrecisionXBT();
      valuePrec = UiUtils::GetAmountPrecisionFX();
      if (security.substr(0, security.find('/')) != product) {
         std::swap(qtyPrec, valuePrec);
      }
      break;
   case bs::network::Asset::Type::PrivateMarket:
      qtyPrec = UiUtils::GetAmountPrecisionCC();
      // special case. display value for XBT with 6 decimals
      valuePrec = 6;
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

      default :
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
   } else {
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

      default :
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
