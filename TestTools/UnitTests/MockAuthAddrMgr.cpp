#include "MockAuthAddrMgr.h"


MockAuthAddrMgr::MockAuthAddrMgr(const std::shared_ptr<spdlog::logger>& logger)
   : AuthAddressManager(logger)
{
   addresses_ = { bs::Address(std::string("2MxeBMYgTeF9XGvgLVLDQg5wW15WiWtGqPf")),
      bs::Address(std::string("2NFWju6yY2UMW8RQ3gPc2bz3CDLNDrfssdp")) };

   defaultAddr_ = addresses_[0];

   for (const auto &addr : addresses_) {
      states_[addr.prefixed()] = AddressVerificationState::Verified;
      bsAddressList_.insert(addr.display<std::string>());
   }
}
