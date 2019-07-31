#include "PasswordDialogData.h"
#include "google/protobuf/any.h"
#include "google/protobuf/map.h"

#include <QDataStream>

using namespace google::protobuf;
using namespace Blocksettle::Communication;

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

QVariant bs::sync::PasswordDialogData::value(const QString &key) const
{
   return values_.value(key);
}

QVariant bs::sync::PasswordDialogData::value(const char *key) const
{
   return value(QString::fromLatin1(key));
}

void bs::sync::PasswordDialogData::setValue(const QString &key, const QVariant &value)
{
   values_.insert(key, value);
   emit dataChanged();
}

void bs::sync::PasswordDialogData::setValue(const char *key, const QVariant &value)
{
   setValue(QString::fromLatin1(key), value);
}

void bs::sync::PasswordDialogData::setValue(const char *key, const char *value)
{
   setValue(key, QString::fromLatin1(value));
}

bool bs::sync::PasswordDialogData::contains(const QString &key)
{
   return values_.contains(key);
}
