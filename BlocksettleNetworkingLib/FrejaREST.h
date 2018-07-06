#ifndef __FREJA_REST_H__
#define __FREJA_REST_H__

#include <atomic>
#include <functional>
#include <unordered_map>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QSslConfiguration>
#include <QTimer>
#include "EncryptionUtils.h"

namespace spdlog {
   class logger;
}
class QNetworkReply;

class FrejaREST : public QObject
{
   Q_OBJECT
public:
   FrejaREST(const std::shared_ptr<spdlog::logger> &);
   ~FrejaREST() noexcept = default;

   void initData(const QByteArray &caCertData, const QByteArray &keyData, const QByteArray &keyPassphrase
      , const QByteArray &certData);

   using SeqNo = unsigned int;
   SeqNo sendInitAuthRequest(const QString &email);
   SeqNo requestAuthStatus(const QString &authRef);
   SeqNo sendSignRequest(const QString &email, const QString &title, const QString &data);
   SeqNo requestSignStatus(const QString &signRef);
   SeqNo cancelSignRequest(const QString &signRef);

signals:
   void repliedInitAuthRequest(SeqNo, const QString &authRef);
   void repliedAuthRequestStatus(SeqNo, const QString &status, const QString &details);
   void repliedInitSignRequest(SeqNo, const QString &signRef);
   void repliedSignRequestStatus(SeqNo, const QString &status, const QByteArray &signature);
   void requestFailed(SeqNo);

private slots:
   void requestFinished(QNetworkReply *);
   void onSslErrors(QNetworkReply *, const QList<QSslError> &errors);

private:
   void init();

private:
   using ReplyProcessor = std::function<bool (QNetworkReply *, const QJsonObject &)>;

   std::shared_ptr<spdlog::logger>  logger_;
   QByteArray        caCertData_;
   QByteArray        keyData_;
   QByteArray        keyPassphrase_;
   QByteArray        certData_;
   const QUrl        baseUrl_;
   const QString     baseAuthPath_;
   const QString     baseSignPath_;
   QNetworkAccessManager   nam_;
   QSslConfiguration       sslConf_;
   std::unordered_map<std::string, ReplyProcessor> processors_;
   std::atomic<SeqNo>   seqNo_;
};


class FrejaAuth : public QObject
{
   Q_OBJECT
public:
   FrejaAuth(const std::shared_ptr<spdlog::logger> &);
   ~FrejaAuth() noexcept = default;

   bool start(const QString &userId);
   void stop();

signals:
   void succeeded(const QString &userId, const QString &details);
   void failed(const QString &userId, const QString &text);
   void statusUpdated(const QString &userId, const QString &status);

private slots:
   void onTimer();
   void onRepliedInitAuthRequest(FrejaREST::SeqNo, const QString &authRef);
   void onRepliedAuthRequestStatus(FrejaREST::SeqNo, const QString &status, const QString &details);
   void onRequestFailed(FrejaREST::SeqNo);

private:
   std::shared_ptr<spdlog::logger>  logger_;
   FrejaREST         freja_;
   QTimer            timer_;
   FrejaREST::SeqNo  reqId_ = 0;
   QString  userId_;
   QString  authRef_;
   QString  status_;
   bool stopped_ = true;
};

class FrejaSign : public QObject
{
   Q_OBJECT
public:                                                           // seconds
   FrejaSign(const std::shared_ptr<spdlog::logger> &, unsigned int pollInterval = 3);
   ~FrejaSign() noexcept = default;

   bool start(const QString &userId, const QString &title, const QString &data);
   void stop(bool cancel = false);

signals:
   void succeeded(SecureBinaryData signature);
   void failed(const QString &text);
   void statusUpdated(const QString &status);

protected:
   virtual void onReceivedSignature(const QByteArray &);

private slots:
   void onTimer();
   void onRepliedInitSignRequest(FrejaREST::SeqNo, const QString &signRef);
   void onRepliedSignRequestStatus(FrejaREST::SeqNo, const QString &status
      , const QByteArray &signature);
   void onRequestFailed(FrejaREST::SeqNo);

protected:
   std::shared_ptr<spdlog::logger>  logger_;

private:
   FrejaREST         freja_;
   QTimer            timer_;
   FrejaREST::SeqNo  reqId_ = 0;
   QString  signRef_;
   QString  status_;
   bool stopped_ = true;
};

class FrejaSignWallet : public FrejaSign
{
   Q_OBJECT
public:
   FrejaSignWallet(const std::shared_ptr<spdlog::logger> &logger, unsigned int pollInterval = 3)
      : FrejaSign(logger, pollInterval) {}

   bool start(const QString &userId, const QString &title, const std::string &walletId);

signals:
   void succeeded(SecureBinaryData password);

protected:
   void onReceivedSignature(const QByteArray &) override;
};

#endif // __FREJA_REST_H__
