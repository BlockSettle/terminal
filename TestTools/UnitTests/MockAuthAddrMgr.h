#ifndef __MOCK_AUTH_ADDR_MGR_H__
#define __MOCK_AUTH_ADDR_MGR_H__

#include "Address.h"
#include "AuthAddressManager.h"
#include "BinaryData.h"


namespace spdlog {
   class logger;
}

class MockAuthAddrMgr : public AuthAddressManager
{
   Q_OBJECT

public:
   MockAuthAddrMgr(const std::shared_ptr<spdlog::logger>& logger);

   BinaryData GetPublicKey(size_t index) override { return BinaryData(); }
   size_t getDefaultIndex() const override { return 0; }

   bool HaveAuthWallet() const override { return false; }
   bool HasAuthAddr() const override { return true; }
   bool HaveOTP() const override { return false; }
   bool IsReady() const override { return true; }

   bool CreateNewAuthAddress() override { return false; }
   bool SubmitForVerification(const bs::Address &) override { return true; }
   bool Verify(const bs::Address &address) override { return true; }
   bool RevokeAddress(const bs::Address &address) override { return true; }

   bool needsOTPpassword() const override { return false; }
   std::vector<bs::Address> GetVerifiedAddressList() const override { return addresses_; }

   void OnDisconnectedFromCeler() override {}
};

#endif // __MOCK_AUTH_ADDR_MGR_H__
