#ifndef __PUBLIC_RESOLVER_H__
#define __PUBLIC_RESOLVER_H__

#include "Address.h"
#include "BinaryData.h"
#include "Script.h"

#include <string>
#include <map>

namespace bs
{
   class PublicResolver : public ResolverFeed
   {
   public:
      PublicResolver(const std::map<bs::Address, BinaryData> &preimageMap);
      PublicResolver(const std::map<std::string, BinaryData> &preimageMap);

      BinaryData getByVal(const BinaryData &addr) override;
      const SecureBinaryData& getPrivKeyForPubkey(const BinaryData &pk) override;
   private:
      std::map<BinaryData, BinaryData>   preimageMap_;
   };
};

#endif // __PUBLIC_RESOLVER_H__
