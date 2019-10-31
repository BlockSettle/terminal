#ifndef __OFFLINE_SIGNER_H__
#define __OFFLINE_SIGNER_H__

#include <vector>

#include "BSErrorCode.h"
#include "HeadlessContainer.h"

namespace bs {
namespace core {
namespace wallet {

std::vector<TXSignRequest> ParseOfflineTXFile(const std::string &data);
bs::error::ErrorCode ExportTxToFile(const TXSignRequest &txSignReq, const QString &fileNamePath);
bs::error::ErrorCode ExportSignedTxToFile(const BinaryData &signedTx, const QString &fileNamePath
   , const std::string &comment = {});

}
}
}


#endif // __OFFLINE_SIGNER_H__
