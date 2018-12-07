#ifndef __AUTH_SIGN_MANAGER_H__
#define __AUTH_SIGN_MANAGER_H__

#include <functional>
#include <memory>

#include <QObject>


namespace spdlog {
   class logger;
}
class SecureBinaryData;
class ApplicationSettings;


class AuthSignManager : public QObject
{
Q_OBJECT

public:
   AuthSignManager(const std::shared_ptr<spdlog::logger>& logger
      , const std::shared_ptr<ApplicationSettings>& appSettings);
   AuthSignManager(const std::shared_ptr<spdlog::logger>& logger)
      : logger_(logger) {}
   ~AuthSignManager() noexcept = default;

   AuthSignManager(const AuthSignManager&) = delete;
   AuthSignManager& operator = (const AuthSignManager&) = delete;

   AuthSignManager(AuthSignManager&&) = delete;
   AuthSignManager& operator = (AuthSignManager&&) = delete;

   using SignedCb = std::function<void (const SecureBinaryData &signature)>;
   bool Sign(const SecureBinaryData &dataToSign, const SignedCb &);

private:
   std::shared_ptr<spdlog::logger>        logger_;
   std::shared_ptr<ApplicationSettings>   applicationSettings_;
};

#endif // __AUTH_SIGN_MANAGER_H__
