#ifndef __TX_INFO_H__
#define __TX_INFO_H__

#include <memory>
#include <QObject>
#include <QStringList>
#include "CoreWallet.h"
#include "bs_signer.pb.h"

namespace bs {
namespace wallet {

// wrapper on bs::wallet::TXSignRequest
class TXInfo : public QObject
{
   Q_OBJECT

   Q_PROPERTY(bool isValid READ isValid NOTIFY dataChanged)
   Q_PROPERTY(int nbInputs READ nbInputs NOTIFY dataChanged)
   Q_PROPERTY(QStringList inputs READ inputs NOTIFY dataChanged)
   Q_PROPERTY(QStringList recipients READ recipients NOTIFY dataChanged)
   Q_PROPERTY(int txVirtSize READ txVirtSize NOTIFY dataChanged)
   Q_PROPERTY(double amount READ amount NOTIFY dataChanged)
   Q_PROPERTY(double total READ total NOTIFY dataChanged)
   Q_PROPERTY(double fee READ fee NOTIFY dataChanged)
   Q_PROPERTY(double changeAmount READ changeAmount NOTIFY dataChanged)
   Q_PROPERTY(double inputAmount READ inputAmount NOTIFY dataChanged)
   Q_PROPERTY(bool hasChange READ hasChange NOTIFY dataChanged)
   Q_PROPERTY(QString txId READ txId WRITE setTxId NOTIFY dataChanged)
   Q_PROPERTY(QString walletId READ walletId NOTIFY dataChanged)

public:
   TXInfo() : QObject(), txReq_() {}
   TXInfo(const bs::core::wallet::TXSignRequest &txReq) : QObject(), txReq_(txReq) { init(); }
   TXInfo(const Blocksettle::Communication::signer::SignTxRequest &txRequest)
      : QObject(), txReq_(getCoreSignTxRequest(txRequest)) { init();}
   TXInfo(const TXInfo &src) : QObject(), txReq_(src.txReq_) { init(); }

   static bs::core::wallet::TXSignRequest getCoreSignTxRequest(const Blocksettle::Communication::signer::SignTxRequest &req);

   bool isValid() const { return txReq_.isValid(); }
   int nbInputs() const { return (int)txReq_.inputs.size(); }
   QStringList inputs() const;
   QStringList recipients() const;
   int txVirtSize() const { return txReq_.estimateTxVirtSize(); }
   double amount() const;
   double total() const { return amount() + fee(); }
   double fee() const { return txReq_.fee / BTCNumericTypes::BalanceDivider; }
   bool hasChange() const { return (txReq_.change.value > 0); }
   double changeAmount() const { return txReq_.change.value / BTCNumericTypes::BalanceDivider; }
   double inputAmount() const;
   QString txId() const { return txId_; }
   void setTxId(const QString &);
   QString walletId() const { return QString::fromStdString(txReq_.walletIds.front()); }

signals:
   void dataChanged();

private:
   void init();

private:
   const bs::core::wallet::TXSignRequest  txReq_;
   QString  txId_;
};

}  //namespace wallet
}  //namespace bs


#endif // __TX_INFO_H__
