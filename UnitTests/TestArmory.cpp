#include <gtest/gtest.h>
#include "AsyncClient.h"
#include "BDVCodec.h"
#include "TestEnv.h"

using namespace ::Codec_BDVCommand;

TEST(TestArmory, CrashOnEmptyAddress)
{
   TestEnv env(StaticLogger::loggerPtr);
   env.requireArmory();
   const auto armoryConn = env.armoryConnection();
   EXPECT_TRUE(armoryConn->isOnline());

   // Exploring various crashes on invalid input data: either empty or non-existing address/wallet
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

   class TestACT : public ArmoryCallbackTarget
   {
   public:
      TestACT(const std::shared_ptr<std::promise<bool>> &promise) : ArmoryCallbackTarget(), prom_(promise) {}
   protected:
      void onLedgerForAddress(const bs::Address &, const std::shared_ptr<AsyncClient::LedgerDelegate> &ledger) override
      {
         prom_->set_value(ledger == nullptr);
      }
   private:
      std::shared_ptr<std::promise<bool>> prom_;
   };
   const auto promPtrLedger = std::make_shared<std::promise<bool>>();
   auto futLedger = promPtrLedger->get_future();
   TestACT act(promPtrLedger);
   act.init(env.armoryConnection().get());
   try {
      EXPECT_TRUE(armoryConn->getLedgerDelegateForAddress("walletId", emptyAddr));
      EXPECT_TRUE(futLedger.get());
   }
   catch (const std::exception &e) {
      std::cout << "getLedgerDelegateForAddress thrown " << e.what() << "\n";
      EXPECT_TRUE(true);
   }
   act.cleanup();

   auto lbdGetOutpointsForAddressesRaw = [bdv=armoryConn->bdv()]
      (std::function<void(ReturnMessage<OutpointBatch>)> callback)
   {
      auto payload = make_unique<WritePayload_Protobuf>();
      auto message = make_unique<BDVCommand>();
      message->set_method(Methods::getOutpointsForAddresses);

      payload->message_ = move(message);

      auto command = dynamic_cast<BDVCommand*>(payload->message_.get());
      command->add_bindata((void *)"", 0);  // use maliciously crafted command to be passed to Armory server
      command->set_height(0);
      command->set_zcid(0);

      auto read_payload = std::make_shared<Socket_ReadPayload>();
      read_payload->callbackReturn_ =
         make_unique<CallbackReturn_AddrOutpoints>(callback);
      bdv->getSocketObject()->pushPayload(move(payload), read_payload);
   };
   const auto promPtrOPRaw = std::make_shared<std::promise<bool>>();
   auto futOPRaw = promPtrOPRaw->get_future();
   auto cbOPRaw = [promPtrOPRaw](ReturnMessage<OutpointBatch> retMsg)
   {
      try {
         retMsg.get();
         promPtrOPRaw->set_value(false);
      }
      catch (...) {
         promPtrOPRaw->set_value(true);
      }
   };
   try {
      lbdGetOutpointsForAddressesRaw(cbOPRaw);
      EXPECT_TRUE(futOPRaw.get());
   }
   catch (...) {
      EXPECT_TRUE(true);
   }
}
