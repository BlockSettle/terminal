#include "MockAuthAddrMgr.h"

namespace {

void newKeyCb(const std::string&, const std::string&, const std::string &
   , const std::shared_ptr<std::promise<bool>> &promise)
{
   promise->set_value(true);
}

} // namespace

MockAuthAddrMgr::MockAuthAddrMgr(const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<ArmoryConnection> &armory)
   : AuthAddressManager(logger, armory, nullptr)
{
   addresses_ = { bs::Address::fromAddressString(std::string("2MxeBMYgTeF9XGvgLVLDQg5wW15WiWtGqPf")),
      bs::Address::fromAddressString(std::string("2NFWju6yY2UMW8RQ3gPc2bz3CDLNDrfssdp")) };

   defaultAddr_ = addresses_[0];

   for (const auto &addr : addresses_) {
      states_[addr] = AddressVerificationState::Verified;
      bsAddressList_.insert(addr.display());
   }
}
