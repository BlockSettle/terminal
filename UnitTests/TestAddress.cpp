#include <gtest/gtest.h>

#include "Address.h"

void testAddress(const std::string &addrStr)
{
   auto addr = bs::Address(addrStr);
   auto recipient = addr.getRecipient(bs::XBTAmount(1.));
   BinaryData script = recipient->getSerializedScript();
   auto recipient2 = ScriptRecipient::deserialize(script);
   auto addr2 = bs::Address::fromRecipient(recipient2);
   EXPECT_EQ(addrStr, addr2);
}

TEST(TestAddress, FromRecipient)
{
   testAddress("2NB6rBycxtMY2GxP8Swfc1VEzGigzbRDHnt");
   testAddress("tb1qtt3kp8ahfceej9srws5fscdnnf2h2e5vaz2lkw");
   testAddress("tb1qn9qtqsg04a8jmc552hxwtl469jjsa7fxrwzxt2v9sn3ycn2xelxqtjut5k");
}
