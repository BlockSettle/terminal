#ifndef __PASSWORD_DIALOG_DATA_H__
#define __PASSWORD_DIALOG_DATA_H__

#include <QObject>
#include <QVariantMap>
#include "headless.pb.h"

namespace bs {
namespace sync {

namespace dialog {
namespace keys {

   class Key
   {
   public:
      explicit constexpr Key(const char *name)
         : name_(name) {}
      QString toString() const { return QString::fromLatin1(name_); }
   private:
      const char *name_;
   };

   extern Key AutoSignCategory;
   extern Key DeliveryUTXOVerified;
   extern Key DialogType;
   extern Key Duration;
   extern Key InputAmount;
   extern Key InputsListVisible;
   extern Key LotSize;
   extern Key Market;
   extern Key NetworkFee;
   extern Key PayOutRevokeType;
   extern Key Price;
   extern Key Product;
   extern Key FxProduct;
   extern Key ProductGroup;
   extern Key Quantity;
   extern Key RecipientsListVisible;
   extern Key RequesterAuthAddress;
   extern Key RequesterAuthAddressVerified;
   extern Key ResponderAuthAddress;
   extern Key ResponderAuthAddressVerified;
   extern Key ReturnAmount;
   extern Key Security;
   extern Key SettlementAddress;
   extern Key SettlementId;
   extern Key SettlementPayInVisible;
   extern Key SettlementPayOutVisible;
   extern Key Side;
   extern Key SigningAllowed;
   extern Key Title;
   extern Key TotalSpentVisible;
   extern Key TotalValue;
   extern Key TransactionAmount;
   extern Key TxInputProduct;
   extern Key WalletId;
   extern Key XBT;

} // namespace keys
} // namespace dialog

class PasswordDialogData : public QObject
{
   Q_OBJECT
   Q_PROPERTY(bool deliveryUTXOVerified READ deliveryUTXOVerified NOTIFY dataChanged)

   Q_PROPERTY(bool requesterAuthAddressVerified READ requesterAuthAddressVerified NOTIFY dataChanged)
   Q_PROPERTY(bool responderAuthAddressVerified READ responderAuthAddressVerified NOTIFY dataChanged)

public:
   PasswordDialogData(QObject *parent = nullptr) : QObject(parent) {}
   PasswordDialogData(const Blocksettle::Communication::Internal::PasswordDialogData &info, QObject *parent = nullptr);
   PasswordDialogData(const PasswordDialogData &src);
   PasswordDialogData(const QVariantMap &values, QObject *parent = nullptr)
      : QObject(parent), values_(values) { }

   Blocksettle::Communication::Internal::PasswordDialogData toProtobufMessage() const;

   Q_INVOKABLE QVariantMap values() const;
   Q_INVOKABLE QStringList keys() const { return values().keys(); }

   Q_INVOKABLE QVariant value(const QString &key) const;
   QVariant value(const char *key) const;

   void setValue(const bs::sync::dialog::keys::Key &key, const QVariant &value);
   void setValue(const bs::sync::dialog::keys::Key &key, const char *value);
   void setValue(const bs::sync::dialog::keys::Key &key, const std::string &value);

   void remove(const bs::sync::dialog::keys::Key &key);

   Q_INVOKABLE bool contains(const QString &key);
   bool contains(const char *key) { return contains(QString::fromLatin1(key)); }
   Q_INVOKABLE void merge(PasswordDialogData *other);

signals:
   void dataChanged();

private:
   void remove(const QString &key);
   void setValue(const QString &key, const QVariant &value);

   void setValues(const QVariantMap &values);

   bool deliveryUTXOVerified() { return value("DeliveryUTXOVerified").toBool(); }

   bool requesterAuthAddressVerified() { return value("RequesterAuthAddressVerified").toBool(); }
   bool responderAuthAddressVerified() { return value("ResponderAuthAddressVerified").toBool(); }

private:
   QVariantMap values_;
};


} // namespace sync
} // namespace bs
#endif // __PASSWORD_DIALOG_DATA_H__
