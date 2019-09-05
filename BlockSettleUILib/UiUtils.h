#ifndef __UI_UTILS_H__
#define __UI_UTILS_H__

#include <QLocale>
#include <QObject>
#include <QString>
#include <QValidator>

#include <memory>
#include "CommonTypes.h"
#include "BTCNumericTypes.h"
#include "ApplicationSettings.h"
#include "CommonTypes.h"

QT_BEGIN_NAMESPACE
class QAbstractItemModel;
QT_END_NAMESPACE

namespace bs {
   namespace sync {
      class WalletsManager;
   }
}
class AuthAddressManager;
class BinaryData;
class PyBlockDataManager;
class QComboBox;
class QDateTime;
class QPixmap;
class SignContainer;
class Tx;

namespace UiUtils
{
   class CommonUiUtilsText : public QObject
   {
      Q_OBJECT
      CommonUiUtilsText() = delete;
   };

   void SetupLocale();

   template <typename T>
   BTCNumericTypes::balance_type amountToBtc(T value)
   {
      return static_cast<BTCNumericTypes::balance_type>(value) / BTCNumericTypes::BalanceDivider;
   }
   template <>
   BTCNumericTypes::balance_type amountToBtc(BTCNumericTypes::balance_type value);

   constexpr int GetAmountPrecisionXBT() { return BTCNumericTypes::default_precision; }
   constexpr int GetAmountPrecisionFX()  { return 2; }
   constexpr int GetAmountPrecisionCC()  { return 0; }

   constexpr int GetPricePrecisionXBT();
   constexpr int GetPricePrecisionFX();
   constexpr int GetPricePrecisionCC();

   QString UnifyValueString(const QString& value);
   QString NormalizeString(const QString& value);

   QValidator::State ValidateDoubleString(QString &input, int &pos, const int decimals);

   template <typename T> QString displayAmount(T value);

   double parseAmountBtc(const QString& text);

   QString displayCurrencyAmount(double value);
   QString displayCCAmount(double value);
   QString displayQuantity(double quantity, const QString& currency);
   QString displayQuantity(double quantity, const std::string& currency);
   QString displayQty(double quantity, const QString &currency);
   QString displayQty(double quantity, const std::string &currency);
   QString displayQty(double qty, const std::string &security, const std::string &product, bs::network::Asset::Type);
   QString displayValue(double qty, const std::string &security, const std::string &product, bs::network::Asset::Type);

   QString displayPriceForAssetType(double price, bs::network::Asset::Type at);
   double  truncatePriceForAsset(double price, bs::network::Asset::Type at);

   QString displayPriceFX(double price);
   QString displayPriceXBT(double price);
   QString displayPriceCC(double price);

   int GetPricePrecisionForAssetType(const bs::network::Asset::Type& assetType);

   // return only number, product string is not included
   QString displayAmountForProduct(double quantity, const QString& product, bs::network::Asset::Type at);

   QString displayDateTime(uint64_t time);
   QString displayDateTime(const QDateTime& datetime);
   QString displayTimeMs(const QDateTime& datetime);

   QString displayAddress(const QString &addr);
   QString displayShortAddress(const QString &addr, const uint maxLength);
   enum WalletDataRole
   {
      WalletIdRole = Qt::UserRole,
      WalletBalanceRole
   };

   // Returns default wallet index (or -1 if empty).
   // Only bitcoin wallets would be used.
   int fillWalletsComboBox(QComboBox* comboBox, const std::shared_ptr<bs::sync::WalletsManager>& walletsManager
      , bool skipWatchingOnly);
   int fillHDWalletsComboBox(QComboBox* comboBox, const std::shared_ptr<bs::sync::WalletsManager>& walletsManager);
   void fillAuthAddressesComboBox(QComboBox* comboBox, const std::shared_ptr<AuthAddressManager>& authAddressManager);

   int selectWalletInCombobox(QComboBox* comboBox, const std::string& walletId);
   std::string getSelectedWalletId(QComboBox* comboBox);

   QPixmap getQRCode(const QString& address, int size = 0);

   void setupIconFont(QObject *parent = nullptr);
   QIcon icon( int character, const QVariantMap& options = QVariantMap() );
   QIcon icon( const QString& name, const QVariantMap& options = QVariantMap() );
   QIcon icon( const char* name, const QVariantMap& options = QVariantMap() );

   void setWrongState(QWidget *widget, bool wrong);

   ApplicationSettings::Setting limitRfqSetting(bs::network::Asset::Type type);
   ApplicationSettings::Setting limitRfqSetting(const QString &name);
   QString marketNameForLimit(ApplicationSettings::Setting s);

   QString modelPath(const QModelIndex &index, QAbstractItemModel *model);

   extern const QLatin1String XbtCurrency;


   //
   // WalletDescriptionValidator
   //

   //! Validator for description of wallet.
   class WalletDescriptionValidator final : public QValidator
   {
   public:
      explicit WalletDescriptionValidator(QObject *parent);

      QValidator::State validate(QString &input, int &pos) const override;
   };
}

#endif // __UI_UTILS_H__
