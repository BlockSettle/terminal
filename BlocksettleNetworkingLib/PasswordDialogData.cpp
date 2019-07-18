#include "PasswordDialogData.h"

#include <QDataStream>

bs::sync::PasswordDialogData::PasswordDialogData(const Blocksettle::Communication::Internal::PasswordDialogData &info, QObject *parent)
   : QObject (parent)
{
   QByteArray ba = QByteArray::fromStdString(info.valuesmap());
   QDataStream stream(&ba, QIODevice::ReadOnly);
   stream >> values_;
}

bs::sync::PasswordDialogData::PasswordDialogData(const bs::sync::PasswordDialogData &src)
{
   setParent(src.parent());
   values_ = src.values_;
}

Blocksettle::Communication::Internal::PasswordDialogData bs::sync::PasswordDialogData::toProtobufMessage() const
{
   QByteArray ba;
   QDataStream stream(&ba, QIODevice::WriteOnly);
   stream << values_;

   Blocksettle::Communication::Internal::PasswordDialogData info;
   info.set_valuesmap(ba.data(), ba.size());

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
