#ifndef __CC_FILE_MANAGER_H__
#define __CC_FILE_MANAGER_H__

#include "CCPubConnection.h"

#include <memory>
#include <vector>

#include <QString>
#include <QVariant>

#include "ZMQ_BIP15X_DataConnection.h"

namespace Blocksettle {
   namespace Communication {
      class GetCCGenesisAddressesResponse;
   }
}

class ApplicationSettings;
class AuthSignManager;
class CelerClient;

class CCFileManager : public CCPubConnection
{
Q_OBJECT
public:
   CCFileManager(const std::shared_ptr<spdlog::logger> &logger, const std::shared_ptr<ApplicationSettings> &appSettings
      , const std::shared_ptr<AuthSignManager> &
      , const std::shared_ptr<ConnectionManager> &, const ZmqBIP15XDataConnection::cbNewKey &cb = nullptr);
   ~CCFileManager() noexcept override = default;

   CCFileManager(const CCFileManager&) = delete;
   CCFileManager& operator = (const CCFileManager&) = delete;
   CCFileManager(CCFileManager&&) = delete;
   CCFileManager& operator = (CCFileManager&&) = delete;

   using CCSecurities = std::vector<bs::network::CCSecurityDef>;
   CCSecurities ccSecurities() const { return ccSecurities_; }
   bool synchronized() const { return syncFinished_; }

   void LoadSavedCCDefinitions();
   void ConnectToCelerClient(const std::shared_ptr<CelerClient> &);

   bool SubmitAddressToPuB(const bs::Address &, uint32_t seed);
   bool wasAddressSubmitted(const bs::Address &);

   bool hasLocalFile() const;

signals:
   void CCSecurityDef(bs::network::CCSecurityDef);
   void CCSecurityId(const std::string& securityId);
   void CCSecurityInfo(QString ccProd, QString ccDesc, unsigned long nbSatoshis, QString genesisAddr);

   void CCAddressSubmitted(const QString);
   void CCInitialSubmitted(const QString);
   void CCSubmitFailed(const QString address, const QString &err);
   void Loaded();
   void LoadingFailed();

private slots:
   void onPubSettingsChanged(int setting, QVariant value);

private:
   void RemoveAndDisableFileSave();

protected:
   void ProcessGenAddressesResponse(const std::string& response, bool sigVerified, const std::string &sig) override;
   void ProcessSubmitAddrResponse(const std::string& response) override;

   bool VerifySignature(const std::string& data, const std::string& signature) const override;

   bool IsTestNet() const override;

   std::string GetPuBHost() const override;
   std::string GetPuBPort() const override;
   std::string GetPuBKey() const override;

private:
   std::shared_ptr<ApplicationSettings>   appSettings_;
   std::shared_ptr<CelerClient>           celerClient_;
   std::shared_ptr<AuthSignManager>       authSignManager_;

   CCSecurities   ccSecurities_;

   // when user changes PuB connection settings - save to file should be disabled.
   // dev build feature only. final release should have single PuB.
   bool saveToFileDisabled_ = false;

   bool syncStarted_ = false;
   bool syncFinished_ = false;

private:
   bool LoadFromFile(const std::string &path);
   bool SaveToFile(const std::string &path, const std::string &signature);
   bool RequestFromPuB();
   void FillFrom(Blocksettle::Communication::GetCCGenesisAddressesResponse *);
};

#endif // __CC_FILE_MANAGER_H__
