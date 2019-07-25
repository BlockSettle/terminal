#ifndef __PASSWORD_DIALOG_DATA_H__
#define __PASSWORD_DIALOG_DATA_H__

#include <QObject>
#include <QVariantMap>
#include "headless.pb.h"

namespace bs {
namespace sync {

class PasswordDialogData : public QObject
{
   Q_OBJECT

public:
   PasswordDialogData(QObject *parent = nullptr) : QObject(parent) {}
   PasswordDialogData(const Blocksettle::Communication::Internal::PasswordDialogData &info, QObject *parent = nullptr);
   PasswordDialogData(const PasswordDialogData &src);
   PasswordDialogData(const QVariantMap &values, QObject *parent = nullptr)
      : QObject(parent), values_(values) { }

   Blocksettle::Communication::Internal::PasswordDialogData toProtobufMessage() const;

   Q_INVOKABLE QVariantMap values() const;

   Q_INVOKABLE QVariant value(const QString &key) const;
   QVariant value(const char *key) const;

   void setValue(const QString &key, const QVariant &value);
   void setValue(const char *key, const QVariant &value);
   void setValue(const char *key, const char *value);

signals:
   void dataChanged();

private:
   void setValues(const QVariantMap &values);

private:
   QVariantMap values_;
};


} // namespace sync
} // namespace bs
#endif // __PASSWORD_DIALOG_DATA_H__
