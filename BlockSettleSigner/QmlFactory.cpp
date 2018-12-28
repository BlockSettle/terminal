#include "QmlFactory.h"
#include "AuthProxy.h"

using namespace bs::hd;
using namespace bs::wallet;


// todo
// check authObject->signWallet results, return null object, emit error signal

AuthSignWalletObject *QmlFactory::createAutheIDSignObject(AutheIDClient::RequestType requestType
                                                          , WalletInfo *walletInfo)
{
   logger_->debug("[QmlFactory] signing {}", walletInfo->walletId().toStdString());
   AuthSignWalletObject *authObject = new AuthSignWalletObject(logger_, this);
   authObject->signWallet(requestType, walletInfo);
   QQmlEngine::setObjectOwnership(authObject, QQmlEngine::JavaScriptOwnership);
   return authObject;
}

AuthSignWalletObject *QmlFactory::createActivateEidObject(const QString &userId
                                                          , WalletInfo *walletInfo)
{
   logger_->debug("[QmlFactory] activate wallet {} for {}", walletInfo->walletId().toStdString(), userId.toStdString());
   AuthSignWalletObject *authObject = new AuthSignWalletObject(logger_, this);
   walletInfo->setEncKeys(QStringList() << (userId + QStringLiteral("::")));
   authObject->signWallet(AutheIDClient::ActivateWallet, walletInfo);
   QQmlEngine::setObjectOwnership(authObject, QQmlEngine::JavaScriptOwnership);
   return authObject;
}

AuthSignWalletObject *QmlFactory::createRemoveEidObject(int index
                                                        , WalletInfo *walletInfo)
{
   logger_->debug("[QmlFactory] remove device for {}, device index: {}", walletInfo->walletId().toStdString(), index);
   AuthSignWalletObject *authObject = new AuthSignWalletObject(logger_, this);
   authObject->removeDevice(index, walletInfo);
   QQmlEngine::setObjectOwnership(authObject, QQmlEngine::JavaScriptOwnership);
   return authObject;
}
