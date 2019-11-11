#include <gtest/gtest.h>
#include "AsyncClient.h"
#include "TestEnv.h"

TEST(TestArmory, CrashOnEmptyAddress)
{
   TestEnv env(StaticLogger::loggerPtr);
   env.requireArmory();
   const auto armoryConn = env.armoryConnection();
   EXPECT_TRUE(armoryConn->isOnline());

   // Explorer various crashes on invalid input data: either empty or non-existing address/wallet
   const bs::Address emptyAddr{};
/*   const auto privKey = CryptoPRNG::generateRandom(32);
   const auto pubKey = CryptoECDSA().ComputePublicKey(privKey, true);
   const auto emptyAddr = bs::Address::fromPubKey(pubKey, AddressEntryType_P2WPKH);*/

   const auto promPtrOPBatch = std::make_shared<std::promise<bool>>();
   auto futOPBatch = promPtrOPBatch->get_future();
   auto cbOPBatch = [promPtrOPBatch](const OutpointBatch &, std::exception_ptr eptr)
   {
      promPtrOPBatch->set_value(eptr != nullptr);
   };
   try {
      EXPECT_TRUE(armoryConn->getOutpointsForAddresses({ emptyAddr.prefixed() }, cbOPBatch));
      EXPECT_TRUE(futOPBatch.get());
   }
   catch (const std::exception &e) {
      std::cout << "getOutputsForAddresses thrown " << e.what() << "\n";
      EXPECT_TRUE(true);
   }

   const auto promPtrLedger = std::make_shared<std::promise<bool>>();
   auto futLedger = promPtrLedger->get_future();
   const auto cbLedgerDelegate = [promPtrLedger](const std::shared_ptr<AsyncClient::LedgerDelegate> &ledger)
   {  // normally ledger should be null for empty address
      promPtrLedger->set_value(ledger == nullptr);
   };
   try {
      EXPECT_TRUE(armoryConn->getLedgerDelegateForAddress("walletId", emptyAddr, cbLedgerDelegate));
      EXPECT_TRUE(futLedger.get());
   }
   catch (const std::exception &e) {
      std::cout << "getLedgerDelegateForAddress thrown " << e.what() << "\n";
      EXPECT_TRUE(true);
   }
}
