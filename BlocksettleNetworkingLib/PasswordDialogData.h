#ifndef __PASSWORD_DIALOG_DATA_H__
#define __PASSWORD_DIALOG_DATA_H__

#include "headless.pb.h"

#ifdef QT_CORE_LIB

#include <QObject>
#include <QVariantMap>

#define DIALOG_KEY(KEYNAME) const static bs::sync::dialog::keys::Key KEYNAME;\
QVariant get##KEYNAME() { return values_.value(KEYNAME.toQString()); }\
void set##KEYNAME(const QVariant &v) { values_.insert(KEYNAME.toQString(), v); emit dataChanged(); }\
Q_INVOKABLE bool has##KEYNAME() { return values_.contains(KEYNAME.toQString()); }\
Q_PROPERTY(QVariant KEYNAME READ get##KEYNAME WRITE set##KEYNAME NOTIFY dataChanged)

#define DIALOG_KEY_BOOL(KEYNAME) const static bs::sync::dialog::keys::Key KEYNAME;\
bool get##KEYNAME() { return values_.value(KEYNAME.toQString()).toBool(); }\
void set##KEYNAME(bool b) { values_.insert(KEYNAME.toQString(), QVariant::fromValue(b)); emit dataChanged(); }\
Q_INVOKABLE bool has##KEYNAME() { return values_.contains(KEYNAME.toQString()); }\
Q_PROPERTY(bool KEYNAME READ get##KEYNAME WRITE set##KEYNAME NOTIFY dataChanged)

#else
#define DIALOG_KEY(KEYNAME) const static bs::sync::dialog::keys::Key KEYNAME;
#define DIALOG_KEY_BOOL(KEYNAME) const static bs::sync::dialog::keys::Key KEYNAME;
#endif

namespace bs {
namespace sync {

namespace dialog {
namespace keys {

   class Key
   {
   public:
      explicit constexpr Key(const char *name)
         : name_(name) {}
      const char *toString() const { return name_; }
   #ifdef QT_CORE_LIB
      QString toQString() const { return QString::fromLatin1(name_); }
   #endif

   private:
      const char *name_;
   };

} // namespace keys
} // namespace dialog

class PasswordDialogData
#ifdef QT_CORE_LIB
   : public QObject
#endif
{
public:
   DIALOG_KEY(AutoSignCategory)
   DIALOG_KEY(AuthAddress)
   DIALOG_KEY_BOOL(DeliveryUTXOVerified)
   DIALOG_KEY(DialogType)
   DIALOG_KEY(Duration)
   DIALOG_KEY(InputAmount)
   DIALOG_KEY(InputsListVisible)
   DIALOG_KEY(LotSize)
   DIALOG_KEY(Market)
   DIALOG_KEY(NetworkFee)
   DIALOG_KEY_BOOL(PayOutRevokeType)
   DIALOG_KEY_BOOL(PayOutType)
   DIALOG_KEY(Price)
   DIALOG_KEY(Product)
   DIALOG_KEY(FxProduct)
   DIALOG_KEY(ProductGroup)
   DIALOG_KEY(Quantity)
   DIALOG_KEY(RecipientsListVisible)
   DIALOG_KEY(RequesterAuthAddress)
   DIALOG_KEY_BOOL(RequesterAuthAddressVerified)
   DIALOG_KEY(ResponderAuthAddress)
   DIALOG_KEY_BOOL(ResponderAuthAddressVerified)
   DIALOG_KEY(ReturnAmount)
   DIALOG_KEY(Security)
   DIALOG_KEY(SettlementAddress)
   DIALOG_KEY(SettlementId)
   DIALOG_KEY_BOOL(SettlementPayInVisible)
   DIALOG_KEY_BOOL(SettlementPayOutVisible)
   DIALOG_KEY(Side)
   DIALOG_KEY_BOOL(SigningAllowed)
   DIALOG_KEY(Title)
   DIALOG_KEY(TotalValue)
   DIALOG_KEY(TransactionAmount)
   DIALOG_KEY(TxInputProduct)
   DIALOG_KEY(WalletId)
   DIALOG_KEY(XBT)

#ifdef QT_CORE_LIB
   Q_OBJECT

public:
   PasswordDialogData(QObject *parent = nullptr) : QObject(parent) {}
   PasswordDialogData(const Blocksettle::Communication::Internal::PasswordDialogData &info, QObject *parent = nullptr);
   PasswordDialogData(const PasswordDialogData &src);
   PasswordDialogData(const QVariantMap &values, QObject *parent = nullptr)
      : QObject(parent), values_(values) { }

   Blocksettle::Communication::Internal::PasswordDialogData toProtobufMessage() const;

   Q_INVOKABLE QVariantMap values() const;
   Q_INVOKABLE QStringList keys() const { return values().keys(); }

   Q_INVOKABLE QVariant value(const bs::sync::dialog::keys::Key &key) const;

   void setValue(const bs::sync::dialog::keys::Key &key, const QVariant &value);
   void setValue(const bs::sync::dialog::keys::Key &key, const char *value);
   void setValue(const bs::sync::dialog::keys::Key &key, const std::string &value);

   void remove(const bs::sync::dialog::keys::Key &key);

   Q_INVOKABLE void merge(PasswordDialogData *other);

signals:
   void dataChanged();

private:
   void remove(const QString &key);
   void setValue(const QString &key, const QVariant &value);
   void setValues(const QVariantMap &values);

private:
   QVariantMap values_;

#endif
};


} // namespace sync
} // namespace bs
#endif // __PASSWORD_DIALOG_DATA_H__
