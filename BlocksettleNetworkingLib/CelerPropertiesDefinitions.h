/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __CELER_PROPERTIES_DEFINITIONS_H__
#define __CELER_PROPERTIES_DEFINITIONS_H__

#include <string>

namespace CelerUserProperties {

static const std::string UserIdPropertyName = "USER_ID";
static const std::string SubmittedBtcAuthAddressListPropertyName = "SUBMITTED_BTC_AUTH_ADDRESS";
static const std::string SubmittedCCAddressListPropertyName = "SUBMITTED_CC_ADDRESS";
static const std::string MarketSessionPropertyName = "marketmerchant.session";
static const std::string SocketAccessPropertyName = "SOCKET_ACCESS";
static const std::string BitcoinParticipantPropertyName = "bitcoin.participant";
static const std::string BitcoinDealerPropertyName = "bitcoin.dealer";

static const std::string EnablePropertyValue = "true";
static const std::string DisabledPropertyValue = "false";

std::string GetCelerPropertyDescription(const std::string& propertyName);

}
// c.  key: bitcoin.dealer value: true (only applicable if the user is enabled as a bitcoin dealer)
// d.  key: bitcoin.participant value: true (enabled for everyone for trading spot fx)

#endif // __CELER_PROPERTIES_DEFINITIONS_H__
