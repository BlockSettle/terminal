#include "StringUtils.h"

#include <botan/hex.h>

namespace bs {

std::string toHex(const std::string &str, bool uppercase)
{
   return Botan::hex_encode(reinterpret_cast<const uint8_t*>(str.data()), str.size(), uppercase);
}

} // namespace bs
