#include <QFile>
#include <QVariant>
#include <QBuffer>
#include <QByteArray>
#include <QPixmap>

#include <spdlog/spdlog.h>

#include "FrejaProxy.h"


FrejaProxy::FrejaProxy(const std::shared_ptr<spdlog::logger> &logger, QObject *parent)
   : QObject(parent), logger_(logger)
{}

FrejaSignWalletObject *FrejaProxy::signWallet(const QString &userId, const QString &title, const QString &walletId)
{
   logger_->debug("[FrejaProxy] signing {} for {}: {}", walletId.toStdString(), userId.toStdString(), title.toStdString());
   return new FrejaSignWalletObject(logger_, userId, title, walletId, this);
}

void FrejaSignWalletObject::cancel()
{
   freja_.stop(true);
}


void FrejaObject::setStatus(const QString &status)
{
   status_ = tr("Freja status: %1").arg(status);
   emit statusChanged();
}


FrejaSignWalletObject::FrejaSignWalletObject(const std::shared_ptr<spdlog::logger> &logger
   , const QString &userId, const QString &title, const QString &walletId, QObject *parent)
   : FrejaObject(parent), freja_(logger, 1)
{
   connect(&freja_, &FrejaSignWallet::succeeded, [this](SecureBinaryData password) {
      emit success(QString::fromStdString(password.toHexStr()));
   });
   connect(&freja_, &FrejaSign::failed, [this](const QString &text) { emit error(text); });
   connect(&freja_, &FrejaSign::statusUpdated, [this](const QString &status) { setStatus(status); });

   freja_.start(userId, title, walletId.toStdString());
}
