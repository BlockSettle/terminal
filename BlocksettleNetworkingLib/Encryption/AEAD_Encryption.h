/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#pragma once

#ifndef AEAD_ENCRYPTION_H
#define AEAD_ENCRYPTION_H

#include "AEAD_Cipher.h"

namespace Encryption
{

   class AEAD_Encryption : public AEAD_Cipher
   {
   public:
      AEAD_Encryption(const std::shared_ptr<spdlog::logger>&);
      ~AEAD_Encryption() = default;

      static std::unique_ptr<AEAD_Encryption> create(const std::shared_ptr<spdlog::logger>& logger);

      void finish(Botan::SecureVector<uint8_t>& data) override;
   };

}

#endif // AEAD_ENCRYPTION_H