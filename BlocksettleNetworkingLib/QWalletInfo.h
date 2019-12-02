/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __BS_QWALLETINFO_H__
#define __BS_QWALLETINFO_H__

#include <QObject>
#include "headless.pb.h"
#include "QSeed.h"

namespace bs {
   namespace core {
      namespace hd {
         class Wallet;
      }
   }
   namespace sync {
      namespace hd {
         class Wallet;
      }
      class Wallet;
      class WalletsManager;
   }

namespace hd {

class WalletInfo : public QObject
{
   Q_OBJECT

   Q_PROPERTY(QString walletId READ walletId WRITE setWalletId NOTIFY walletChanged)
   Q_PROPERTY(QString rootId READ rootId WRITE setRootId NOTIFY walletChanged)
   Q_PROPERTY(QString name READ name WRITE setName NOTIFY walletChanged)
   Q_PROPERTY(QString desc READ desc WRITE setDesc NOTIFY walletChanged)
   Q_PROPERTY(QStringList encKeys READ encKeys WRITE setEncKeys NOTIFY walletChanged)

   // currently we using only single encription type for whole wallet
   Q_PROPERTY(int encType READ encType WRITE setEncType NOTIFY walletChanged)

public:
   WalletInfo(QObject *parent = nullptr) : QObject(parent) {}

   // used in headless container
   WalletInfo(const QString &rootId, const std::vector<bs::wallet::EncryptionType> &encTypes
              , const std::vector<BinaryData> &encKeys, const bs::wallet::KeyRank &keyRank);

   WalletInfo(const Blocksettle::Communication::headless::GetHDWalletInfoResponse &response);

   // used in signer
   WalletInfo(std::shared_ptr<bs::core::hd::Wallet> hdWallet, QObject *parent = nullptr);
   WalletInfo(const std::shared_ptr<bs::sync::WalletsManager> &
      , const std::shared_ptr<bs::sync::hd::Wallet> &, QObject *parent = nullptr);
   WalletInfo(const std::shared_ptr<bs::sync::WalletsManager> &
      , const std::shared_ptr<bs::sync::Wallet> &, QObject *parent = nullptr);

   WalletInfo(const WalletInfo &other);
   WalletInfo& operator= (const WalletInfo &other);

   static WalletInfo fromDigitalBackup(const QString &filename);
   void initFromWallet(const bs::sync::Wallet *, const std::string &rootId = {});
   void initFromRootWallet(const std::shared_ptr<bs::core::hd::Wallet> &);
   void initEncKeys(const std::shared_ptr<bs::core::hd::Wallet> &rootWallet);
   void initFromRootWallet(const std::shared_ptr<bs::sync::hd::Wallet> &);
   void initEncKeys(const std::shared_ptr<bs::sync::hd::Wallet> &rootWallet);

   QString walletId() const { return walletId_; }
   void setWalletId(const QString &walletId);

   QString desc() const { return desc_; }
   void setDesc(const QString &);

   QString name() const { return name_; }
   void setName(const QString &);
   void setName(const std::string &name) { setName(QString::fromStdString(name)); }

   QString rootId() const { return rootId_; }
   void setRootId(const QString &);
   void setRootId(const std::string &rootId) { setRootId(QString::fromStdString(rootId)); }

   QList<QString> encKeys() const { return encKeys_; }
   void setEncKeys(const QList<QString> &encKeys);
   void setEncKeys(const std::vector<BinaryData> &encKeys);

   Q_INVOKABLE QList<bs::wallet::EncryptionType> encTypes() const { return encTypes_; }
   void setEncTypes(const QList<bs::wallet::EncryptionType> &encTypes);
   void setEncTypes(const std::vector<bs::wallet::EncryptionType> &encTypes);

   // just set encKeys and encTypes from PasswordData vector
   void setPasswordData(const std::vector<bs::wallet::PasswordData> &passwordData);

   // currently we supports only single enc type for whole wallet: either Password or eID Auth
   // this function returns encType based on first passwordDataList_ value
   Q_INVOKABLE bs::wallet::EncryptionType encType();
   void setEncType(int encType);

   // currently we supports only single account for whole wallet, thus email stored in encKeys_.at(0)
   Q_INVOKABLE QString email() const;

   bs::wallet::KeyRank keyRank() const { return keyRank_; }
   void setKeyRank(const bs::wallet::KeyRank &keyRank) { keyRank_ = keyRank; }

   bool isEidAuthOnly() const;
   bool isPasswordOnly() const;

signals:
   void walletChanged();

private:
   std::shared_ptr<bs::sync::WalletsManager> walletsMgr_;
   QString    walletId_;
   QString    rootId_;
   QString    name_, desc_;
   QList<QString> encKeys_;
   QList<bs::wallet::EncryptionType> encTypes_;
   bs::wallet::KeyRank keyRank_;
};


}  //namespace hd
}  //namespace bs


#endif // __BS_QWALLETINFO_H__
