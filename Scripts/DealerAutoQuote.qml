/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import bs.terminal 1.0

BSQuoteReqReply {		// Class name for Dealer replies. Don't change it!
/* implied properties: listed here for reference
    quoteReq: {
        READONLY property string requestId
        READONLY property string product
        READONLY property bool isBuy
        READONLY property real quantity
        READONLY property int  assetType
    }
    READONLY property string security
    property real   expirationInSec	// Time in seconds when the Quote Request is expired

    property real   indicBid
    property real   indicAsk
    property real   lastPrice
    property real   bestPrice
*/

//  accountBalance(product) // Call this method to obtain balance for the product
//  product()               // Returns opposite product as compared to quoteReq's one
//  sendQuoteReply(double price)
//  pullQuoteReply()

    property var prevSendPrice: 0

    function checkBalance(value, product) {
        var balance = accountBalance(product)
        if (value > balance) {
            log("Not enough balance for " + product + ": " + value + " > " + balance)
            return false
        }
        return true
    }

    onExpirationInSecChanged: {		// Ticks every 0.5s until QuoteReq expiry
        if ((prevSendPrice == 0) && (expirationInSec < 7)) {
            prevSendPrice = (Math.random().toPrecision(3) * 10.0).toPrecision(2)
            log("No quotes sent - sending random one with price " + prevSendPrice)
            sendQuoteReply(prevSendPrice)
        }
    }

    onBestPriceChanged: {
        if (quoteReq.assetType == 3)    return  // Don't reply on CC

        var price = 0
        if (quoteReq.isBuy)  price = bestPrice * 0.999
        else                 price = bestPrice * 1.001

        sendQuoteReply(price)
        prevSendPrice = price
    }

    onIndicBidChanged: {
        if (quoteReq.assetType == 3)    return  // Don't reply on CC
        if (quoteReq.isBuy)  return
        var price = 0
        if ((prevSendPrice == 0) || ((indicBid - prevSendPrice) < indicBid * 0.01)) {
            price = indicBid * 0.99
        }
        if ((price > 0) && checkBalance(quoteReq.quantity * price, product())) {
            sendQuoteReply(price)
            prevSendPrice = price
        }
    }

    onIndicAskChanged: {
        if (quoteReq.assetType == 3)    return  // Don't reply on CC
        if (!quoteReq.isBuy)  return
        var price = 0
        if ((prevSendPrice == 0) || ((prevSendPrice - indicAsk) < indicAsk * 0.01)) {
            price = indicAsk * 1.01
        }
        if ((price > 0) && checkBalance(quoteReq.quantity, quoteReq.product)) {
            sendQuoteReply(price)
            prevSendPrice = price
        }
    }

    onSendFailed: {	// Invoked when sending of reply has failed
        log("Sending failed: " + reason)
    }
}
