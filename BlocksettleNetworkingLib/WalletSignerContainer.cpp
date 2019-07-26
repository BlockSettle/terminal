#include "WalletSignerContainer.h"


WalletSignerContainer::WalletSignerContainer(const std::shared_ptr<spdlog::logger> &logger, OpMode opMode)
  : SignContainer(logger, opMode)
{}
