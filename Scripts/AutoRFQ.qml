/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import bs.terminal 1.0

RFQScript {
    property var tradeRules: [
        'XBT/EUR',
        'EUR/GBP',
        'EUR/USD'
    ]

/* This example script just resends RFQ on its expiration for selected instruments */

    onStarted: {
        log("Started")
        for (var i = 0; i < tradeRules.length; i++) {
           var amount = (i == 0) ? Math.random() / 23 : Math.random() * 230
           var rfq = sendRFQ(tradeRules[i], false, amount)
           log("Sent " + rfq.id + " " + rfq.security + " " + rfq.amount)
        }
    }

    onExpired: {
       var rfq = activeRFQ(id)
       if (rfq === undefined) {
          log("RFQ " + id + " not exists")
          return
       }
       sendRFQ(rfq.security, rfq.buy, rfq.amount)
       rfq.stop()
    }

    onAccepted: {
       var rfq = activeRFQ(id)
       if (rfq === undefined) {
          log("RFQ " + id + " not exists")
          return
       }
       rfq.stop()
    }
}
