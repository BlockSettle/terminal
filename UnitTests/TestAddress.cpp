#include <gtest/gtest.h>

#include "Address.h"

TEST(TestAddress, FromRecipientNestedSW)
{
   auto addr = bs::Address("2NB6rBycxtMY2GxP8Swfc1VEzGigzbRDHnt");
   auto recipient = addr.getRecipient(bs::XBTAmount(1.));
   BinaryData script = recipient->getSerializedScript();
   auto recipient2 = ScriptRecipient::deserialize(script);
   auto addr2 = bs::Address::fromRecipient(recipient2);
   ASSERT_EQ(addr.display(), addr2);
}
