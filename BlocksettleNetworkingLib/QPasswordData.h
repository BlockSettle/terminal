/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __BS_QPASSWORDDATA_H__
#define __BS_QPASSWORDDATA_H__

#include <QObject>
#include "WalletEncryption.h"


namespace bs {
namespace wallet {

//// PasswordData::password might be either binary ot plain text depends of wallet encryption type
//// for QEncryptionType::Password - it's plain text
//// for QEncryptionType::Auth - it's binary
//// textPassword and binaryPassword properties provides Qt interfaces for PasswordData::password usable in QML
//// password size limited to 32 bytes
class QPasswordData : public QObject, public PasswordData {
   Q_OBJECT
public:
   enum QEncryptionType
   {
      Unencrypted,
      Password,
      Auth,
   };
   Q_ENUMS(QEncryptionType)

   Q_PROPERTY(QString textPassword READ textPassword WRITE setTextPassword NOTIFY passwordChanged)
   Q_PROPERTY(SecureBinaryData binaryPassword READ binaryPassword WRITE setBinaryPassword NOTIFY passwordChanged)
   Q_PROPERTY(QEncryptionType encType READ getEncType WRITE setEncType NOTIFY encTypeChanged)
   Q_PROPERTY(QString encKey READ getEncKey WRITE setEncKey NOTIFY encKeyChanged)

   QPasswordData(QObject *parent = nullptr) : QObject(parent), PasswordData() {}

   // copy constructors and operator= uses parent implementation
   QPasswordData(const PasswordData &other) : PasswordData(other){}
   QPasswordData(const QPasswordData &other) : PasswordData(static_cast<PasswordData>(other)) {}
   QPasswordData& operator= (const QPasswordData &other) { PasswordData::operator=(other); return *this;}

   QString                 textPassword() const             { return QString::fromStdString(password.toBinStr()); }
   SecureBinaryData        binaryPassword() const           { return password; }
   QEncryptionType         getEncType() const               { return QEncryptionType(metaData.encType); }
   QString                 getEncKey() const                { return QString::fromStdString(metaData.encKey.toBinStr()); }

   void setTextPassword    (const QString &pw)              { password =  SecureBinaryData(pw.toStdString()); emit passwordChanged(); }
   void setBinaryPassword  (const SecureBinaryData &data)   { password =  data; emit passwordChanged(); }
   void setEncType         (QEncryptionType e)              { metaData.encType = EncryptionType(e); emit encTypeChanged(e); }
   void setEncKey          (const QString &e)               { metaData.encKey =  SecureBinaryData(e.toStdString()); emit encKeyChanged(e); }

signals:
   void passwordChanged();
   void encTypeChanged(QEncryptionType);
   void encKeyChanged(QString);
};


} //namespace wallet
}  //namespace bs


#endif // __BS_QPASSWORDDATA_H__
