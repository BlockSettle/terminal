/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#pragma once

#ifndef IES_ENCRYPTION_H
#define IES_ENCRYPTION_H

#include "Cipher.h"

namespace Encryption
{

   class IES_Encryption : public Cipher
   {
   public:
      IES_Encryption(const std::shared_ptr<spdlog::logger>&);
      ~IES_Encryption() = default;

      static std::unique_ptr<IES_Encryption> create(const std::shared_ptr<spdlog::logger>& logger);

      void finish(Botan::SecureVector<uint8_t>& data) override;
   };

}

#endif // IES_ENCRYPTION_H