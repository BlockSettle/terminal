#include "MockAuthAddrMgr.h"

MockAuthAddrMgr::MockAuthAddrMgr(const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<ArmoryConnection> &armory)
   : AuthAddressManager(logger, armory)
{
   addresses_ = { bs::Address::fromAddressString(std::string("2MxeBMYgTeF9XGvgLVLDQg5wW15WiWtGqPf")),
      bs::Address::fromAddressString(std::string("2NFWju6yY2UMW8RQ3gPc2bz3CDLNDrfssdp")) };

   defaultAddr_ = addresses_[0];

   for (const auto &addr : addresses_) {
      states_[addr] = AddressVerificationState::Verified;
      bsAddressList_.insert(addr.display());
   }
}
