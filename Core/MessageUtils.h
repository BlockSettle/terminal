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
      class MatchingMessage_Order;
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

   } // namespace message
} // namespace bs

#endif	// MESSAGE_UTILS_H
