#ifndef __FREJA_PROXY_H__
#define __FREJA_PROXY_H__

#include <memory>
#include <QObject>
#include "EncryptionUtils.h"
#include "FrejaREST.h"


namespace spdlog {
   class logger;
}


class FrejaObject : public QObject
{
   Q_OBJECT
   Q_PROPERTY(QString status READ status NOTIFY statusChanged)

public:
   FrejaObject(QObject *parent = nullptr)
      : QObject(parent) {}

signals:
   void statusChanged();
   void error(QString errMsg) const;

protected:
   QString status() const { return status_; }
   void setStatus(const QString &);

private:
   QString  status_;
};

class FrejaSignWalletObject : public FrejaObject
{
   Q_OBJECT

public:
   FrejaSignWalletObject() : FrejaObject(nullptr), freja_(nullptr) {}
   FrejaSignWalletObject(const std::shared_ptr<spdlog::logger> &, const QString &userId
      , const QString &title, const QString &walletId, QObject *parent = nullptr);

   Q_INVOKABLE void cancel();

signals:
   void success(QString password) const;

private:
   FrejaSignWallet   freja_;
};


class FrejaProxy : public QObject
{
   Q_OBJECT
public:
   FrejaProxy(const std::shared_ptr<spdlog::logger> &, QObject *parent = nullptr);

   Q_INVOKABLE FrejaSignWalletObject *signWallet(const QString &userId, const QString &title
      , const QString &walletId);

private:
   std::shared_ptr<spdlog::logger>  logger_;
};

#endif // __FREJA_PROXY_H__
