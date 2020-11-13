/*

***********************************************************************************
* Copyright (C) 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef MESSAGE_UTILS_H
#define MESSAGE_UTILS_H

#include "Message/Bus.h"
#include "Message/Envelope.h"
#include "CommonTypes.h"

namespace BlockSettle {
   namespace Terminal {
      class RFQ;
      class Quote;
      class IncomingRFQ;
      class MatchingMessage_Order;
      class ReplyToRFQ;
   }
}

namespace bs {
   namespace message {

      bs::network::RFQ fromMsg(const BlockSettle::Terminal::RFQ&);
      void toMsg(const bs::network::RFQ&, BlockSettle::Terminal::RFQ*);

      bs::network::Quote fromMsg(const BlockSettle::Terminal::Quote&);
      void toMsg(const bs::network::Quote&, BlockSettle::Terminal::Quote*);

      bs::network::Order fromMsg(const BlockSettle::Terminal::MatchingMessage_Order&);
      void toMsg(const bs::network::Order&, BlockSettle::Terminal::MatchingMessage_Order*);

      bs::network::QuoteReqNotification fromMsg(const BlockSettle::Terminal::IncomingRFQ&);
      void toMsg(const bs::network::QuoteReqNotification&, BlockSettle::Terminal::IncomingRFQ*);

      bs::network::QuoteNotification fromMsg(const BlockSettle::Terminal::ReplyToRFQ&);
      void toMsg(const bs::network::QuoteNotification&, BlockSettle::Terminal::ReplyToRFQ*);

   } // namespace message
} // namespace bs

#endif	// MESSAGE_UTILS_H
