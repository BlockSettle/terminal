#ifndef __SETTLEMENT_INFO_H__
#define __SETTLEMENT_INFO_H__

#include <QObject>
#include "headless.pb.h"

namespace bs {
namespace sync {

class SettlementInfo : public QObject
{
   Q_OBJECT

   Q_PROPERTY(QString productGroup READ productGroup NOTIFY dataChanged)
   Q_PROPERTY(QString security READ security NOTIFY dataChanged)
   Q_PROPERTY(QString product READ product NOTIFY dataChanged)
   Q_PROPERTY(QString side READ side NOTIFY dataChanged)
   Q_PROPERTY(QString quantity READ quantity NOTIFY dataChanged)
   Q_PROPERTY(QString price READ price NOTIFY dataChanged)
   Q_PROPERTY(QString totalValue READ totalValue NOTIFY dataChanged)

   Q_PROPERTY(QString payment READ payment NOTIFY dataChanged)
   Q_PROPERTY(QString genesisAddress READ genesisAddress NOTIFY dataChanged)

   Q_PROPERTY(QString requesterAuthAddress READ requesterAuthAddress NOTIFY dataChanged)
   Q_PROPERTY(QString responderAuthAddress READ responderAuthAddress NOTIFY dataChanged)
   Q_PROPERTY(QString wallet READ wallet NOTIFY dataChanged)
   Q_PROPERTY(QString transaction READ transaction NOTIFY dataChanged)

   Q_PROPERTY(QString transactionAmount READ transactionAmount NOTIFY dataChanged)
   Q_PROPERTY(QString networkFee READ networkFee NOTIFY dataChanged)
   Q_PROPERTY(QString totalSpent READ totalSpent NOTIFY dataChanged)

public:
   SettlementInfo(QObject *parent = nullptr) : QObject(parent) {}
   SettlementInfo(const Blocksettle::Communication::Internal::SettlementInfo &info, QObject *parent = nullptr);
   SettlementInfo(const SettlementInfo &src);

   Blocksettle::Communication::Internal::SettlementInfo toProtobufMessage() const;

   QString productGroup() const;
   QString security() const;
   QString product() const;
   QString side() const;
   QString quantity() const;
   QString price() const;
   QString totalValue() const;

   QString payment() const;
   QString genesisAddress() const;

   QString requesterAuthAddress() const;
   QString responderAuthAddress() const;
   QString wallet() const;
   QString transaction() const;

   QString transactionAmount() const;
   QString networkFee() const;
   QString totalSpent() const;

   void setProductGroup(const QString &productGroup);
   void setSecurity(const QString &security);
   void setProduct(const QString &product);
   void setSide(const QString &side);
   void setQuantity(const QString &quantity);
   void setPrice(const QString &price);
   void setTotalValue(const QString &totalValue);

   void setPayment(const QString &payment);
   void setGenesisAddress(const QString &genesisAddress);

   void setRequesterAuthAddress(const QString &requesterAuthAddress);
   void setResponderAuthAddress(const QString &responderAuthAddress);

   void setWallet(const QString &wallet);
   void setTransaction(const QString &transaction);
   void setTransactionAmount(const QString &transactionAmount);

   void setNetworkFee(const QString &networkFee);
   void setTotalSpent(const QString &totalSpent);

signals:
   void dataChanged();

private:
   // Details
   QString productGroup_;
   QString security_;
   QString product_;
   QString side_;
   QString quantity_;
   QString price_;
   QString totalValue_;

   // Settlement details
   QString payment_;
   QString genesisAddress_;

   // XBT Settlement details
   QString requesterAuthAddress_;
   QString responderAuthAddress_;
   QString wallet_;
   QString transaction_;

   // Transaction details
   QString transactionAmount_;
   QString networkFee_;
   QString totalSpent_;
};


} // namespace sync
} // namespace bs
#endif // __SETTLEMENT_INFO_H__
