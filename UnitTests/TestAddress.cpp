#include <gtest/gtest.h>

#include "Address.h"

TEST(TestAddress, TestResolverSerialize)
{
   auto addr = bs::Address("2NB6rBycxtMY2GxP8Swfc1VEzGigzbRDHnt");
   auto resolver = addr.getRecipient(bs::XBTAmount(1.));
   BinaryData script = resolver->getSerializedScript();
   auto resolver2 = ScriptRecipient::deserialize(script);
   auto addr2 = bs::Address::fromRecipient(resolver2);
   ASSERT_EQ(addr.display(), addr2.display());
}
