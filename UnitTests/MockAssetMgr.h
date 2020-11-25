/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __MOCK_ASSET_MGR_H__
#define __MOCK_ASSET_MGR_H__

#include <memory>
#include <string>
#include <vector>
#include "AssetManager.h"


namespace spdlog {
   class logger;
}
namespace bs {
   namespace sync {
      class Wallet;
   }
}

class MockAssetManager : public AssetManager
{
   Q_OBJECT

public:
   MockAssetManager(const std::shared_ptr<spdlog::logger>& logger)
      : AssetManager(logger, nullptr, nullptr, nullptr) {}
   void init() override;

   std::vector<std::string> privateShares(bool forceExt) override;
   double getBalance(const std::string& currency, bool includeZc = false, const std::shared_ptr<bs::sync::Wallet> &wallet = nullptr) const override;
};

#endif // __MOCK_ASSET_MGR_H__
