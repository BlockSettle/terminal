#ifndef __OFFLINE_SIGNER_H__
#define __OFFLINE_SIGNER_H__

#include <vector>
#include "HeadlessContainer.h"

std::vector<bs::core::wallet::TXSignRequest> ParseOfflineTXFile(const std::string &data);

#endif // __OFFLINE_SIGNER_H__
