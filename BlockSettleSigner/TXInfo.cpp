#include "TXInfo.h"
#include "HDWallet.h"
#include "WalletsManager.h"


static WalletInfo::EncryptionType mapEncType(bs::wallet::EncryptionType encType)
{
   switch (encType) {
   case bs::wallet::EncryptionType::Password:      return WalletInfo::Password;
   case bs::wallet::EncryptionType::Freja:         return WalletInfo::Freja;
   case bs::wallet::EncryptionType::Unencrypted:
   default:    return WalletInfo::Unencrypted;
   }
}

WalletInfo::WalletInfo(const std::shared_ptr<WalletsManager> &walletsMgr, const std::string &walletId
   , QObject *parent)
   : QObject(parent), id_(QString::fromStdString(walletId))
{
   const auto &wallet = walletsMgr->GetWalletById(walletId);
   if (wallet) {
      const auto &rootWallet = walletsMgr->GetHDRootForLeaf(wallet->GetWalletId());
      initFromWallet(wallet.get(), rootWallet->getWalletId());
   }
   else {
      const auto &hdWallet = walletsMgr->GetHDWalletById(walletId);
      if (!hdWallet) {
         throw std::runtime_error("failed to find wallet id " + walletId);
      }
      initFromRootWallet(hdWallet);
   }
}

void WalletInfo::initFromWallet(const bs::Wallet *wallet, const std::string &rootId)
{
   id_ = QString::fromStdString(wallet->GetWalletId());
   rootId_ = QString::fromStdString(rootId);
   name_ = QString::fromStdString(wallet->GetWalletName());
   encType_ = wallet->encryptionTypes().empty() ? WalletInfo::Unencrypted : mapEncType(wallet->encryptionTypes()[0]);
   encKey_ = wallet->encryptionKeys().empty() ? QString() : QString::fromStdString(wallet->encryptionKeys()[0].toBinStr());
   emit dataChanged();
}

void WalletInfo::initFromRootWallet(const std::shared_ptr<bs::hd::Wallet> &rootWallet)
{
   id_ = QString::fromStdString(rootWallet->getWalletId());
   name_ = QString::fromStdString(rootWallet->getName());
   rootId_ = QString::fromStdString(rootWallet->getWalletId());
   encType_ = rootWallet->encryptionTypes().empty() ? WalletInfo::Unencrypted : mapEncType(rootWallet->encryptionTypes()[0]);
   encKey_ = rootWallet->encryptionKeys().empty() ? QString() : QString::fromStdString(rootWallet->encryptionKeys()[0].toBinStr());
}

void WalletInfo::setId(const QString &id)
{
   if (id_ == id) {
      return;
   }
   id_ = id;
   emit dataChanged();
}

void WalletInfo::setRootId(const QString &rootId)
{
   if (rootId_ == rootId) {
      return;
   }
   rootId_ = rootId;
   emit dataChanged();
}

void WalletInfo::setName(const QString &name)
{
   if (name_ == name) {
      return;
   }
   name_ = name;
   emit dataChanged();
}

void WalletInfo::setEncKey(const QString &encKey)
{
   if (encKey_ == encKey) {
      return;
   }
   encKey_ = encKey;
   emit dataChanged();
}

void WalletInfo::setEncType(int encType)
{
   encType_ = static_cast<EncryptionType>(encType);
   emit dataChanged();
}


TXInfo::TXInfo(const std::shared_ptr<WalletsManager> &walletsMgr, const bs::wallet::TXSignRequest &txReq)
   : QObject(), walletsMgr_(walletsMgr), txReq_(txReq)
{
   init();
}

void TXInfo::init()
{
   if (txReq_.wallet) {
      const auto &rootWallet = walletsMgr_->GetHDRootForLeaf(txReq_.wallet->GetWalletId());
      if (rootWallet) {
         walletInfo_ = new WalletInfo(this);
         walletInfo_->initFromWallet(txReq_.wallet, rootWallet->getWalletId());
      }
      else {
         throw std::runtime_error("no root wallet for leaf " + txReq_.wallet->GetWalletId());
      }
   }
   else {
      walletInfo_ = new WalletInfo(walletsMgr_, txReq_.walletId, this);
   }
   emit dataChanged();
}

QStringList TXInfo::recvAddresses() const
{
   QStringList result;
   for (const auto &recip : txReq_.recipients) {
      result.push_back(bs::Address::fromRecipient(recip).display());
   }
   return result;
}

double TXInfo::amount() const
{
   uint64_t result = 0;
   for (const auto &recip : txReq_.recipients) {
      result += recip->getValue();
   }
   return result / BTCNumericTypes::BalanceDivider;
}
