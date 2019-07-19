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

   Blocksettle::Communication::Internal::PasswordDialogData toProtobufMessage() const;

   QVariantMap values() const;
   void setValues(const QVariantMap &values);

   QVariant value(const QString &key) const;
   QVariant value(const char *key) const;

   void setValue(const QString &key, const QVariant &value);
   void setValue(const char *key, const QVariant &value);
   void setValue(const char *key, const char *value);

signals:
   void dataChanged();

private:
   QVariantMap values_;
};


} // namespace sync
} // namespace bs
#endif // __SETTLEMENT_INFO_H__
