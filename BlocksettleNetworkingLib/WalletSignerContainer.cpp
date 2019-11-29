/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "WalletSignerContainer.h"


WalletSignerContainer::WalletSignerContainer(const std::shared_ptr<spdlog::logger> &logger, OpMode opMode)
  : SignContainer(logger, opMode)
{}
