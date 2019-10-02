#ifdef QT_CORE_LIB

#include "PasswordDialogData.h"
#include "google/protobuf/any.h"
#include "google/protobuf/map.h"

#include <QDataStream>

using namespace google::protobuf;
using namespace Blocksettle::Communication;

#define DIALOG_KEY_INIT(KEYNAME) const bs::sync::dialog::keys::Key bs::sync::PasswordDialogData::KEYNAME = bs::sync::dialog::keys::Key(#KEYNAME);

DIALOG_KEY_INIT(AutoSignCategory)
DIALOG_KEY_INIT(AuthAddress)
DIALOG_KEY_INIT(DeliveryUTXOVerified)
DIALOG_KEY_INIT(DialogType)
DIALOG_KEY_INIT(Duration)
DIALOG_KEY_INIT(InputAmount)
DIALOG_KEY_INIT(InputsListVisible)
DIALOG_KEY_INIT(LotSize)
DIALOG_KEY_INIT(Market)
DIALOG_KEY_INIT(NetworkFee)
DIALOG_KEY_INIT(PayOutRevokeType)
DIALOG_KEY_INIT(PayOutType)
DIALOG_KEY_INIT(Price)
DIALOG_KEY_INIT(Product)
DIALOG_KEY_INIT(FxProduct)
DIALOG_KEY_INIT(ProductGroup)
DIALOG_KEY_INIT(Quantity)
DIALOG_KEY_INIT(RecipientsListVisible)
DIALOG_KEY_INIT(RequesterAuthAddress)
DIALOG_KEY_INIT(RequesterAuthAddressVerified)
DIALOG_KEY_INIT(ResponderAuthAddress)
DIALOG_KEY_INIT(ResponderAuthAddressVerified)
DIALOG_KEY_INIT(ReturnAmount)
DIALOG_KEY_INIT(Security)
DIALOG_KEY_INIT(SettlementAddress)
DIALOG_KEY_INIT(SettlementId)
DIALOG_KEY_INIT(SettlementPayInVisible)
DIALOG_KEY_INIT(SettlementPayOutVisible)
DIALOG_KEY_INIT(Side)
DIALOG_KEY_INIT(SigningAllowed)
DIALOG_KEY_INIT(Title)
DIALOG_KEY_INIT(TotalValue)
DIALOG_KEY_INIT(TransactionAmount)
DIALOG_KEY_INIT(TxInputProduct)
DIALOG_KEY_INIT(WalletId)
DIALOG_KEY_INIT(XBT)

Any toPbVariant(const QVariant& v)
{
   Any any;

   if (v.type() == QVariant::Map) {
      Internal::MapMessage map;
      QMapIterator<QString, QVariant> i(v.toMap());
      while (i.hasNext()) {
          i.next();
          const auto &p = MapPair<string, Any>(i.key().toStdString(), toPbVariant(i.value()));
          map.mutable_value_map()->insert(p);
      }
      any.PackFrom(map);
      return any;
   }

   if (v.type() == QVariant::List) {
      Internal::RepeatedMessage list;
      QListIterator<QVariant> i(v.toList());
      while (i.hasNext()) {
         Any *newAny = list.add_value_array();
         *newAny = toPbVariant(i.next());
      }
      any.PackFrom(list);
      return any;
   }

   Internal::AnyMessage msg;
   if (v.type() == QVariant::Bool) {
      msg.set_value_bool(v.toBool());
   }
   else if (v.type() == QVariant::String) {
      msg.set_value_string(v.toString().toStdString());
   }
   else if (v.type() == QVariant::Int) {
      msg.set_value_int32(v.toInt());
   }
   else if (v.type() == QVariant::UInt) {
      msg.set_value_uint32(v.toUInt());
   }
   else if (v.type() == QVariant::LongLong) {
      msg.set_value_int64(v.toLongLong());
   }
   else if (v.type() == QVariant::ULongLong) {
      msg.set_value_uint64(v.toULongLong());
   }
   else if (v.type() == QVariant::Double) {
      msg.set_value_double(v.toDouble());
   }
   else if (v.type() == QVariant::ByteArray) {
      const auto &ba = v.toByteArray();
      msg.set_value_bytes(ba.data(), ba.size());
   }

   any.PackFrom(msg);
   return any;
}

QVariant fromPbVariant(const Any& v)
{
   QVariant variant;

   if (v.Is<Internal::MapMessage>()) {
      Internal::MapMessage map;
      v.UnpackTo(&map);

      QVariantMap variantMap;

      for (auto i = map.value_map().cbegin(); i != map.value_map().cend(); ++i) {
         variantMap.insert(QString::fromStdString(i->first), fromPbVariant(i->second));
      }
      return variant.fromValue<QVariantMap>(variantMap);
   }

   if (v.Is<Internal::RepeatedMessage>()) {
      Internal::RepeatedMessage list;
      v.UnpackTo(&list);

      QVariantList variantList;

      for (auto i = list.value_array().cbegin(); i != list.value_array().cend(); ++i) {
         variantList.append(fromPbVariant(*i));
      }
      return variant.fromValue<QVariantList>(variantList);
   }

   if (v.Is<Internal::AnyMessage>()) {
      Internal::AnyMessage msg;
      v.UnpackTo(&msg);


      if (msg.value_case() == Internal::AnyMessage::ValueCase::kValueAny) {
         return fromPbVariant(msg.value_any());
      }
      else if (msg.value_case() == Internal::AnyMessage::ValueCase::kValueBool) {
         return QVariant(msg.value_bool());
      }
      else if (msg.value_case() == Internal::AnyMessage::ValueCase::kValueString) {
         return QVariant(QString::fromStdString(msg.value_string()));
      }
      else if (msg.value_case() == Internal::AnyMessage::ValueCase::kValueInt32) {
         return QVariant(msg.value_int32());
      }
      else if (msg.value_case() == Internal::AnyMessage::ValueCase::kValueUint32) {
         return QVariant(msg.value_uint32());
      }
      else if (msg.value_case() == Internal::AnyMessage::ValueCase::kValueInt64) {
         return QVariant(qint64(msg.value_int64()));
      }
      else if (msg.value_case() == Internal::AnyMessage::ValueCase::kValueUint64) {
         return QVariant(quint64(msg.value_uint64()));
      }
      else if (msg.value_case() == Internal::AnyMessage::ValueCase::kValueFloat) {
         return QVariant(msg.value_float());
      }
      else if (msg.value_case() == Internal::AnyMessage::ValueCase::kValueDouble) {
         return QVariant(msg.value_double());
      }
      else if (msg.value_case() == Internal::AnyMessage::ValueCase::kValueBytes) {
         return QVariant(QByteArray::fromStdString(msg.value_bytes()));
      }
   }

   return QVariant();
}


bs::sync::PasswordDialogData::PasswordDialogData(const Blocksettle::Communication::Internal::PasswordDialogData &info, QObject *parent)
   : QObject (parent)
{
   for (const MapPair<string, Any> &mapPair : info.valuesmap()){
      values_.insert(QString::fromStdString(mapPair.first), fromPbVariant(mapPair.second));
   }
}

bs::sync::PasswordDialogData::PasswordDialogData(const bs::sync::PasswordDialogData &src)
{
   setParent(src.parent());
   values_ = src.values_;
}

Blocksettle::Communication::Internal::PasswordDialogData bs::sync::PasswordDialogData::toProtobufMessage() const
{
   Blocksettle::Communication::Internal::PasswordDialogData info;

   QMapIterator<QString, QVariant> i(values_);
   while (i.hasNext()) {
       i.next();
       const auto &p = MapPair<string, Any>(i.key().toStdString(), toPbVariant(i.value()));
       info.mutable_valuesmap()->insert(p);
   }

   return info;
}

QVariantMap bs::sync::PasswordDialogData::values() const
{
   return values_;
}

void bs::sync::PasswordDialogData::setValues(const QVariantMap &values)
{
   values_ = values;
   emit dataChanged();
}

QVariant bs::sync::PasswordDialogData::value(const bs::sync::dialog::keys::Key &key) const
{
   return values_.value(key.toQString());
}

void bs::sync::PasswordDialogData::setValue(const QString &key, const QVariant &value)
{
   values_.insert(key, value);
   emit dataChanged();
}

void bs::sync::PasswordDialogData::setValue(const bs::sync::dialog::keys::Key &key, const QVariant &value)
{
   setValue(key.toQString(), value);
}

void bs::sync::PasswordDialogData::setValue(const bs::sync::dialog::keys::Key &key, const char *value)
{
   setValue(key, QString::fromLatin1(value));
}

void bs::sync::PasswordDialogData::setValue(const bs::sync::dialog::keys::Key &key, const std::string &value)
{
   setValue(key, QString::fromStdString(value));
}

void bs::sync::PasswordDialogData::remove(const QString &key)
{
   values_.remove(key);
   emit dataChanged();
}

void bs::sync::PasswordDialogData::remove(const bs::sync::dialog::keys::Key &key)
{
   remove(key.toQString());
   emit dataChanged();
}

void bs::sync::PasswordDialogData::merge(PasswordDialogData *other)
{
   auto it = other->values_.begin();
   while (it != other->values_.end()) {
      values_.insert(it.key(), it.value());
      it++;
   }
   if (!other->values_.isEmpty()) {
      emit dataChanged();
   }
}

#endif
