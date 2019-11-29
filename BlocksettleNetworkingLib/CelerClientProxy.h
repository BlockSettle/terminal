/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef CELER_CLIENT_PROXY_H
#define CELER_CLIENT_PROXY_H

#include "BaseCelerClient.h"
#include "BsClient.h"

class CelerClientProxy : public BaseCelerClient
{
public:
   CelerClientProxy(const std::shared_ptr<spdlog::logger> &logger, bool userIdRequired = true);
   ~CelerClientProxy() override;

   bool LoginToServer(BsClient *client, const std::string& login, const std::string& email);
private:
   void onSendData(CelerAPI::CelerMessageType messageType, const std::string &data) override;

   BsClient *client_{};
};

#endif
