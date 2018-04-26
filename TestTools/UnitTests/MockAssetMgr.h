#ifndef __MOCK_ASSET_MGR_H__
#define __MOCK_ASSET_MGR_H__

#include <memory>
#include <string>
#include <vector>
#include "AssetManager.h"


namespace spdlog {
   class logger;
}

class MockAssetManager : public AssetManager
{
   Q_OBJECT

public:
   MockAssetManager(const std::shared_ptr<spdlog::logger>& logger)
      : AssetManager(logger, nullptr, nullptr, nullptr) {}
   void init() override;

   std::vector<std::string> privateShares(bool forceExt) override;
   double getBalance(const std::string& currency, const std::shared_ptr<bs::Wallet> &wallet = nullptr) const override;
};

#endif // __MOCK_ASSET_MGR_H__
