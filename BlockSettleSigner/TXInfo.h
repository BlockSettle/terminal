#ifndef __TX_INFO_H__
#define __TX_INFO_H__

#include <memory>
#include <QObject>
#include <QStringList>
#include "MetaData.h"

namespace bs {
   class Wallet;
   namespace hd {
      class Wallet;
   }
}
class WalletsManager;


class WalletInfo : public QObject
{
   Q_OBJECT
   Q_PROPERTY(QString id READ id WRITE setId NOTIFY dataChanged)
   Q_PROPERTY(QString name READ name WRITE setName NOTIFY dataChanged)
   Q_PROPERTY(QString rootId READ rootId WRITE setRootId NOTIFY dataChanged)
   Q_PROPERTY(EncryptionType encType READ encType WRITE setEncType NOTIFY dataChanged)
   Q_PROPERTY(QString encKey READ encKey WRITE setEncKey NOTIFY dataChanged)

public:
   WalletInfo(QObject *parent = nullptr) : QObject(parent) {}
   WalletInfo(const std::shared_ptr<WalletsManager> &, const std::string &walletId, QObject *parent = nullptr);

   enum EncryptionType {
      Unencrypted,
      Password,
      Freja
   };
   Q_ENUMS(EncryptionType)

   void initFromWallet(const bs::Wallet *, const std::string &rootId = {});

   QString id() const { return id_; }
   void setId(const QString &);
   QString name() const { return name_; }
   void setName(const QString &);
   QString rootId() const { return rootId_; }
   void setRootId(const QString &);
   EncryptionType encType() const { return encType_; }
   void setEncType(int);
   QString encKey() const { return encKey_; }
   void setEncKey(const QString &);

signals:
   void dataChanged();

private:
   void initFromRootWallet(const std::shared_ptr<bs::hd::Wallet> &);

private:
   QString  id_;
   QString  rootId_;
   QString  name_;
   QString  encKey_;
   EncryptionType encType_ = Unencrypted;
};

class TXInfo : public QObject
{
   Q_OBJECT

   Q_PROPERTY(bool isValid READ isValid NOTIFY dataChanged)
   Q_PROPERTY(int nbInputs READ nbInputs NOTIFY dataChanged)
   Q_PROPERTY(QStringList recvAddresses READ recvAddresses NOTIFY dataChanged)
   Q_PROPERTY(int txSize READ txSize NOTIFY dataChanged)
   Q_PROPERTY(double amount READ amount NOTIFY dataChanged)
   Q_PROPERTY(double total READ total NOTIFY dataChanged)
   Q_PROPERTY(double fee READ fee NOTIFY dataChanged)
   Q_PROPERTY(double changeAmount READ changeAmount NOTIFY dataChanged)
   Q_PROPERTY(bool hasChange READ hasChange NOTIFY dataChanged)
   Q_PROPERTY(WalletInfo *wallet READ wallet NOTIFY dataChanged)

public:
   TXInfo() : QObject(), txReq_() {}
   TXInfo(const std::shared_ptr<WalletsManager> &, const bs::wallet::TXSignRequest &);
   TXInfo(const TXInfo &src) : QObject(), walletsMgr_(src.walletsMgr_), txReq_(src.txReq_) { init(); }

   bool isValid() const { return txReq_.isValid(); }
   int nbInputs() const { return (int)txReq_.inputs.size(); }
   QStringList recvAddresses() const;
   int txSize() const { return (int)txReq_.estimateTxSize(); }
   double amount() const;
   double total() const { return amount() + fee(); }
   double fee() const { return txReq_.fee / BTCNumericTypes::BalanceDivider; }
   bool hasChange() const { return (txReq_.change.value > 0); }
   double changeAmount() const { return txReq_.change.value / BTCNumericTypes::BalanceDivider; }
   WalletInfo *wallet() const { return walletInfo_; }

signals:
   void dataChanged();

private:
   void init();

private:
   std::shared_ptr<WalletsManager>  walletsMgr_;
   const bs::wallet::TXSignRequest  txReq_;
   WalletInfo  *  walletInfo_;
};

#endif // __TX_INFO_H__
