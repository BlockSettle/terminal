#ifndef __CC_FILE_MANAGER_H__
#define __CC_FILE_MANAGER_H__

#include <memory>
#include <vector>
#include <QObject>
#include <QString>
#include "CommonTypes.h"
#include "OTPManager.h"


namespace spdlog {
   class logger;
}
namespace Blocksettle {
   namespace Communication {
      class GetCCGenesisAddressesResponse;
   }
}
class ApplicationSettings;
class ConnectionManager;
class CelerClient;
class OTPManager;

class CCFileManager : public QObject
{
Q_OBJECT
public:
   CCFileManager(const std::shared_ptr<spdlog::logger> &logger, const std::shared_ptr<ApplicationSettings> &appSettings
      , const std::shared_ptr<OTPManager> &);
   ~CCFileManager() noexcept = default;

   CCFileManager(const CCFileManager&) = delete;
   CCFileManager& operator = (const CCFileManager&) = delete;
   CCFileManager(CCFileManager&&) = delete;
   CCFileManager& operator = (CCFileManager&&) = delete;

   using CCSecurities = std::vector<bs::network::CCSecurityDef>;
   CCSecurities ccSecurities() const { return ccSecurities_; }

   void LoadData();
   void ConnectToPublicBridge(const std::shared_ptr<ConnectionManager> &
      , const std::shared_ptr<CelerClient> &);

   bool SubmitAddressToPuB(const bs::Address &, uint32_t seed, OTPManager::cbPassword);
   bool wasAddressSubmitted(const bs::Address &);
   bool needsOTPpassword() const;

signals:
   void CCSecurityDef(bs::network::CCSecurityDef);
   void CCSecurityInfo(QString ccProd, QString ccDesc, unsigned long nbSatoshis, QString genesisAddr);
   void CCAddressSubmitted(const QString);
   void Loaded();
   void LoadingFailed();

private:
   std::shared_ptr<spdlog::logger>        logger_;
   std::shared_ptr<ApplicationSettings>   appSettings_;
   std::shared_ptr<ConnectionManager>     connectionManager_;
   std::shared_ptr<CelerClient>           celerClient_;
   std::shared_ptr<OTPManager>            otpManager_;
   CCSecurities   ccSecurities_;
   int            currentRev_ = 0;

private:
   bool LoadFromFile(const std::string &path);
   bool SaveToFile(const std::string &path, const std::string &signature);
   bool RequestFromPuB();
   void FillFrom(Blocksettle::Communication::GetCCGenesisAddressesResponse *);

   bool SubmitRequestToPB(const std::string& name, const std::string& data);
   void OnDataReceived(const std::string& data);
   void ProcessGenAddressesResponse(const std::string& response, bool sigVerified, const std::string &sig);
   void ProcessSubmitAddrResponse(const std::string& response, bool sigVerified);
   void ProcessErrorResponse(const std::string& responseString) const;
};

#endif // __CC_FILE_MANAGER_H__
