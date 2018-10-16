#ifndef __CC_FILE_MANAGER_H__
#define __CC_FILE_MANAGER_H__

#include "CCPubConnection.h"
#include "OTPManager.h"

#include <memory>
#include <vector>

#include <QString>

namespace Blocksettle {
   namespace Communication {
      class GetCCGenesisAddressesResponse;
   }
}

class ApplicationSettings;
class CelerClient;
class OTPManager;

class CCFileManager : public CCPubConnection
{
Q_OBJECT
public:
   CCFileManager(const std::shared_ptr<spdlog::logger> &logger, const std::shared_ptr<ApplicationSettings> &appSettings
      , const std::shared_ptr<OTPManager>&
      , const std::shared_ptr<ConnectionManager>&);
   ~CCFileManager() noexcept override = default;

   CCFileManager(const CCFileManager&) = delete;
   CCFileManager& operator = (const CCFileManager&) = delete;
   CCFileManager(CCFileManager&&) = delete;
   CCFileManager& operator = (CCFileManager&&) = delete;

   using CCSecurities = std::vector<bs::network::CCSecurityDef>;
   CCSecurities ccSecurities() const { return ccSecurities_; }

   void LoadSavedCCDefinitions();
   void ConnectToCelerClient(const std::shared_ptr<CelerClient> &);

   bool SubmitAddressToPuB(const bs::Address &, uint32_t seed, OTPManager::cbPassword);
   bool wasAddressSubmitted(const bs::Address &);
   bool needsOTPpassword() const;

   bs::wallet::EncryptionType GetOtpEncType() const { return otpManager_->GetEncType(); }
   QString GetOtpEncKey() const { return otpManager_->GetEncKey(); }
   QString GetOtpId() const { return otpManager_->GetShortId(); }

signals:
   void CCAddressSubmitted(const QString);
   void Loaded();
   void LoadingFailed();

protected:
   void ProcessGenAddressesResponse(const std::string& response, bool sigVerified, const std::string &sig) override;
   void ProcessSubmitAddrResponse(const std::string& response) override;

   bool VerifySignature(const std::string& data, const std::string& signature) const override;

   std::string GetPuBHost() const override;
   std::string GetPuBPort() const override;
   std::string GetPuBKey() const override;

private:
   std::shared_ptr<ApplicationSettings>   appSettings_;
   std::shared_ptr<CelerClient>           celerClient_;
   std::shared_ptr<OTPManager>            otpManager_;

   CCSecurities   ccSecurities_;

private:
   bool LoadFromFile(const std::string &path);
   bool SaveToFile(const std::string &path, const std::string &signature);
   bool RequestFromPuB();
   void FillFrom(Blocksettle::Communication::GetCCGenesisAddressesResponse *);
};

#endif // __CC_FILE_MANAGER_H__
