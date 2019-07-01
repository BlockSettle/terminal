#include "SettlementInfo.h"

bs::sync::SettlementInfo::SettlementInfo(const Blocksettle::Communication::Internal::SettlementInfo &info, QObject *parent)
   : QObject (parent)
{
   productGroup_ = QString::fromUtf8(info.productgroup().c_str());
   security_ = QString::fromUtf8(info.security().c_str());
   product_ = QString::fromUtf8(info.product().c_str());
   side_ = QString::fromUtf8(info.side().c_str());
   quantity_ = QString::fromUtf8(info.quantity().c_str());
   price_ = QString::fromUtf8(info.price().c_str());
   totalValue_ = QString::fromUtf8(info.totalvalue().c_str());
}

bs::sync::SettlementInfo::SettlementInfo(const bs::sync::SettlementInfo &src)
{
   setParent(src.parent());

   productGroup_ = src.productGroup();
   security_ = src.security();
   product_ = src.product();
   side_ = src.side();
   quantity_ = src.quantity();
   price_ = src.price();
   totalValue_ = src.totalValue();

   payment_ = src.payment();
   genesisAddress_ = src.genesisAddress();

   requesterAuthAddress_ = src.requesterAuthAddress();
   responderAuthAddress_ = src.responderAuthAddress();
   wallet_ = src.wallet();
   transaction_ = src.transaction();

   transactionAmount_ = src.transactionAmount();
   networkFee_ = src.networkFee();
   totalSpent_ = src.totalSpent();
}

Blocksettle::Communication::Internal::SettlementInfo bs::sync::SettlementInfo::toProtobufMessage() const
{
   Blocksettle::Communication::Internal::SettlementInfo info;
   info.set_productgroup(productGroup_.toUtf8());
   info.set_security(security_.toUtf8());
   info.set_product(product_.toUtf8());
   info.set_side(side_.toUtf8());
   info.set_quantity(quantity_.toUtf8());
   info.set_price(price_.toUtf8());
   info.set_totalvalue(totalValue_.toUtf8());

   return info;
}

QString bs::sync::SettlementInfo::totalSpent() const
{
   return totalSpent_;
}

void bs::sync::SettlementInfo::setTotalSpent(const QString &totalSpent)
{
   totalSpent_ = totalSpent;
}

void bs::sync::SettlementInfo::setNetworkFee(const QString &networkFee)
{
   networkFee_ = networkFee;
}

void bs::sync::SettlementInfo::setTransactionAmount(const QString &transactionAmount)
{
   transactionAmount_ = transactionAmount;
}

void bs::sync::SettlementInfo::setTransaction(const QString &transaction)
{
   transaction_ = transaction;
}

void bs::sync::SettlementInfo::setWallet(const QString &wallet)
{
   wallet_ = wallet;
}

void bs::sync::SettlementInfo::setResponderAuthAddress(const QString &responderAuthAddress)
{
   responderAuthAddress_ = responderAuthAddress;
}

void bs::sync::SettlementInfo::setRequesterAuthAddress(const QString &requesterAuthAddress)
{
   requesterAuthAddress_ = requesterAuthAddress;
}

void bs::sync::SettlementInfo::setGenesisAddress(const QString &genesisAddress)
{
   genesisAddress_ = genesisAddress;
}

void bs::sync::SettlementInfo::setPayment(const QString &payment)
{
   payment_ = payment;
}

void bs::sync::SettlementInfo::setTotalValue(const QString &totalValue)
{
   totalValue_ = totalValue;
}

void bs::sync::SettlementInfo::setPrice(const QString &price)
{
   price_ = price;
}

void bs::sync::SettlementInfo::setQuantity(const QString &quantity)
{
   quantity_ = quantity;
}

void bs::sync::SettlementInfo::setSide(const QString &side)
{
   side_ = side;
}

void bs::sync::SettlementInfo::setProduct(const QString &product)
{
   product_ = product;
}

void bs::sync::SettlementInfo::setSecurity(const QString &security)
{
   security_ = security;
}

void bs::sync::SettlementInfo::setProductGroup(const QString &productGroup)
{
   productGroup_ = productGroup;
}

QString bs::sync::SettlementInfo::networkFee() const
{
   return networkFee_;
}

QString bs::sync::SettlementInfo::transactionAmount() const
{
   return transactionAmount_;
}

QString bs::sync::SettlementInfo::transaction() const
{
   return transaction_;
}

QString bs::sync::SettlementInfo::wallet() const
{
   return wallet_;
}

QString bs::sync::SettlementInfo::responderAuthAddress() const
{
   return responderAuthAddress_;
}

QString bs::sync::SettlementInfo::requesterAuthAddress() const
{
   return requesterAuthAddress_;
}

QString bs::sync::SettlementInfo::genesisAddress() const
{
   return genesisAddress_;
}

QString bs::sync::SettlementInfo::payment() const
{
   return payment_;
}

QString bs::sync::SettlementInfo::totalValue() const
{
   return totalValue_;
}

QString bs::sync::SettlementInfo::price() const
{
   return price_;
}

QString bs::sync::SettlementInfo::quantity() const
{
   return quantity_;
}

QString bs::sync::SettlementInfo::side() const
{
   return side_;
}

QString bs::sync::SettlementInfo::product() const
{
   return product_;
}

QString bs::sync::SettlementInfo::security() const
{
   return security_;
}

QString bs::sync::SettlementInfo::productGroup() const
{
   return productGroup_;
}
