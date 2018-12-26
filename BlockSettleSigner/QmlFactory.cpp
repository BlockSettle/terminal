#include "QmlFactory.h"
#include "AuthProxy.h"

using namespace bs::hd;
using namespace bs::wallet;


AuthSignWalletObject *QmlFactory::createAutheIDSignObject(AutheIDClient::RequestType requestType
                                                          , WalletInfo *walletInfo)
{
   logger_->debug("[QmlFactory] signing {}", walletInfo->walletId().toStdString());
   AuthSignWalletObject *authObject = new AuthSignWalletObject(logger_, this);
   authObject->signWallet(requestType, walletInfo);
   return authObject;
}

AuthSignWalletObject *QmlFactory::createActivateEidObject(const QString &userId
                                                          , WalletInfo *walletInfo)
{
   logger_->debug("[QmlFactory] activate wallet {} for {}", walletInfo->walletId().toStdString(), userId.toStdString());
   AuthSignWalletObject *authObject = new AuthSignWalletObject(logger_, this);
   walletInfo->setEncKeys(QStringList() << (userId + QStringLiteral("::")));
   authObject->signWallet(AutheIDClient::ActivateWallet, walletInfo);
   return authObject;
}

AuthSignWalletObject *QmlFactory::createRemoveEidObject(int index
                                                        , WalletInfo *walletInfo)
{
   logger_->debug("[QmlFactory] remove device for {}, device index: {}", walletInfo->walletId().toStdString(), index);
   AuthSignWalletObject *authObject = new AuthSignWalletObject(logger_, this);
   authObject->removeDevice(index, walletInfo);
   return authObject;
}
