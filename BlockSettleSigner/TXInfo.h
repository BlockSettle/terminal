#ifndef __TX_INFO_H__
#define __TX_INFO_H__

#include <memory>
#include <QObject>
#include <QStringList>
#include "MetaData.h"

class WalletsManager;


class TXInfo : public QObject
{
   Q_OBJECT
   Q_PROPERTY(bool isValid READ isValid NOTIFY dataChanged)
   Q_PROPERTY(QString sendingWallet READ sendingWallet NOTIFY dataChanged)
   Q_PROPERTY(QString walletId READ walletId NOTIFY dataChanged)
   Q_PROPERTY(int nbInputs READ nbInputs NOTIFY dataChanged)
   Q_PROPERTY(QStringList recvAddresses READ recvAddresses NOTIFY dataChanged)
   Q_PROPERTY(int txSize READ txSize NOTIFY dataChanged)
   Q_PROPERTY(double amount READ amount NOTIFY dataChanged)
   Q_PROPERTY(double total READ total NOTIFY dataChanged)
   Q_PROPERTY(double fee READ fee NOTIFY dataChanged)

public:
   TXInfo() : QObject(), txReq_() {}
   TXInfo(const std::shared_ptr<WalletsManager> &, const bs::wallet::TXSignRequest &);
   TXInfo(const TXInfo &src) : QObject(), walletsMgr_(src.walletsMgr_), txReq_(src.txReq_) { init(); }

   bool isValid() const { return txReq_.isValid(); }
   QString sendingWallet() const { return walletName_; }
   QString walletId() const { return QString::fromStdString(txReq_.walletId); }
   int nbInputs() const { return (int)txReq_.inputs.size(); }
   QStringList recvAddresses() const;
   int txSize() const { return (int)txReq_.estimateTxSize(); }
   double amount() const;
   double total() const { return amount() + fee(); }
   double fee() const { return txReq_.fee / BTCNumericTypes::BalanceDivider; }

signals:
   void dataChanged();

private:
   void init();

private:
   std::shared_ptr<WalletsManager>  walletsMgr_;
   const bs::wallet::TXSignRequest  txReq_;
   QString  walletName_;
};

#endif // __TX_INFO_H__
