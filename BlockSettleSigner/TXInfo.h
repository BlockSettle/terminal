#ifndef __TX_INFO_H__
#define __TX_INFO_H__

#include <memory>
#include <QObject>
#include <QStringList>
#include "MetaData.h"
//#include "HDWallet.h"


namespace bs {
   //class Wallet;
   namespace hd {
      class Wallet;
      class WalletInfo;
   }
}
class WalletsManager;

class TXInfo : public QObject
{
   Q_OBJECT

   Q_PROPERTY(bool isValid READ isValid NOTIFY dataChanged)
   Q_PROPERTY(int nbInputs READ nbInputs NOTIFY dataChanged)
   Q_PROPERTY(QStringList recvAddresses READ recvAddresses NOTIFY dataChanged)
   Q_PROPERTY(int txVirtSize READ txVirtSize NOTIFY dataChanged)
   Q_PROPERTY(double amount READ amount NOTIFY dataChanged)
   Q_PROPERTY(double total READ total NOTIFY dataChanged)
   Q_PROPERTY(double fee READ fee NOTIFY dataChanged)
   Q_PROPERTY(double changeAmount READ changeAmount NOTIFY dataChanged)
   Q_PROPERTY(double inputAmount READ inputAmount NOTIFY dataChanged)
   Q_PROPERTY(bool hasChange READ hasChange NOTIFY dataChanged)
   Q_PROPERTY(bs::hd::WalletInfo *walletInfo READ walletInfo NOTIFY dataChanged)
   Q_PROPERTY(QString txId READ txId NOTIFY dataChanged)

public:
   TXInfo() : QObject(), txReq_() {}
   TXInfo(const std::shared_ptr<WalletsManager> &, const bs::wallet::TXSignRequest &);
   TXInfo(const TXInfo &src) : QObject(), walletsMgr_(src.walletsMgr_), txReq_(src.txReq_) { init(); }

   bool isValid() const { return txReq_.isValid(); }
   int nbInputs() const { return (int)txReq_.inputs.size(); }
   QStringList recvAddresses() const;
   int txVirtSize() const { return (int)txReq_.estimateTxVirtSize(); }
   double amount() const;
   double total() const { return amount() + fee(); }
   double fee() const { return txReq_.fee / BTCNumericTypes::BalanceDivider; }
   bool hasChange() const { return (txReq_.change.value > 0); }
   double changeAmount() const { return txReq_.change.value / BTCNumericTypes::BalanceDivider; }
   bs::hd::WalletInfo *walletInfo() const { return walletInfo_; }
   double inputAmount() const;
   QString txId() const { return QString::fromStdString(txReq_.txId().toBinStr()); }

signals:
   void dataChanged();

private:
   void init();

private:
   std::shared_ptr<WalletsManager>  walletsMgr_;
   const bs::wallet::TXSignRequest  txReq_;
   bs::hd::WalletInfo  *walletInfo_;
};

#endif // __TX_INFO_H__
