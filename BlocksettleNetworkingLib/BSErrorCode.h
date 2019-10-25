#ifndef __BS_ERROR_CODE_H__
#define __BS_ERROR_CODE_H__

namespace bs {
   namespace error {
      enum class ErrorCode
      {
         // General error codes
         NoError = 0,
         FailedToParse,
         WalletNotFound,
         WrongAddress,
         MissingPassword,
         InvalidPassword,
         MissingAuthKeys,
         MissingSettlementWallet,
         MissingAuthWallet,
         InternalError,

         // TX signing error codes
         TxInvalidRequest,
         TxCanceled,
         TxSpendLimitExceed,
         TxRequestFileExist,
         TxFailedToOpenRequestFile,
         TxFailedToWriteRequestFile,

         // Change wallet error codes
         WalletFailedRemoveLastEidDevice,

         // Other codes
         AutoSignDisabled
      };
   }
}

#endif
