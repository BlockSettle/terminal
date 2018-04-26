import bs.terminal 1.0

BSQuoteReqReply {    // Class name for Dealer replies. Don't change it!
/* implied properties: listed here for reference
    quoteReq: {
        READONLY property string requestId
        READONLY property string product
        READONLY property bool isBuy
        READONLY property real quantity
        READONLY property int  assetType
    }
    READONLY property string security
    property real   expirationInSec  // Time in seconds when the Quote Request is expired

    property real   indicBid
    property real   indicAsk
    property real   lastPrice
    property real   bestPrice
*/

//  accountBalance(product) // Call this method to obtain balance for the product
//  product()               // Returns opposite product as compared to quoteReq's one
//  sendQuoteReply(double price)
//  pullQuoteReply()

    //
    property var tradeRules: {
        'BLK/XBT': { max: 1.01, min: 0.99, intervalBid: 0.0001, intervalAsk: 0.0001 },
        'EUR/GBP': { max: 0.995, min: 0.98, intervalBid: 0.0001, intervalAsk: 0.0001 },
        'EUR/JPY': { max: 1.01, min: 0.99, intervalBid: 0.0001, intervalAsk: 0.0001 },
        'EUR/SEK': { max: 1.01, min: 0.99, intervalBid: 0.0001, intervalAsk: 0.0001 },
        'GBP/JPY': { max: 1.01, min: 0.99, intervalBid: 0.0001, intervalAsk: 0.0001 },
        'GBP/SEK': { max: 1.01, min: 0.99, intervalBid: 0.0001, intervalAsk: 0.0001 },
        'JPY/SEK': { max: 1.01, min: 0.99, intervalBid: 0.0001, intervalAsk: 0.0001 },
        'XBT/EUR': { max: 1.01, min: 0.99, intervalBid: 0.0001, intervalAsk: 0.0001 },
        'XBT/GBP': { max: 1.01, min: 0.99, intervalBid: 0.0001, intervalAsk: 0.0001 },
        'XBT/JPY': { max: 1.01, min: 0.99, intervalBid: 0.0001, intervalAsk: 0.0001 },
        'XBT/SEK': { max: 1.01, min: 0.99, intervalBid: 0.0001, intervalAsk: 0.0001 }
    }

    property var prevSendPrice: 0
    property var delayedSendPrice: 0
    property var initialIndicBid: 0
    property var initialIndicAsk: 0

    function checkPrice(price, tradeRule) {
        // check [min,max] bounds
        if (initialIndicBid !== 0.0 && (price < (initialIndicBid * tradeRule.min))) {
            log('#onBestPriceChanged() rejected price change by tradeRule.min bound: ' + tradeRule.min + ' for product ' + quoteReq.product)
            return false
        } else if (initialIndicAsk !== 0.0 && (price > (initialIndicAsk * tradeRule.max))) {
            log('#onBestPriceChanged() rejected price change by tradeRule.max bound: ' + tradeRule.max + ' for product ' + quoteReq.product)
            return false
        }

        if (!price || price < 0) return false
        return true
    }

    function checkBalance(value, product) {
        var balance = accountBalance(product)
        if (value > balance) {
            log("Not enough balance for " + product + ": " + value + " > " + balance)
            return false
        }
        return true
    }

    function sendPrice(price) {
        prevSendPrice = price
        sendQuoteReply(price)
//        delayedSendPrice = price
    }

    onExpirationInSecChanged: {    // Ticks every 0.5s until QuoteReq expiry
        if (delayedSendPrice) {
            prevSendPrice = delayedSendPrice
            sendQuoteReply(delayedSendPrice)
            delayedSendPrice = 0
        }
    }

    onBestPriceChanged: {
        var tradeRule = tradeRules[security]
        log('#onBestPriceChanged() tradeRule: ' + tradeRule + ' for security: ' + security)
        if (!tradeRule) return // this script does not setted up for such product

        var price = 0
        if (quoteReq.isBuy)  price = bestPrice * (1.0 - tradeRule.intervalBid)
        else                 price = bestPrice * (1.0 + tradeRule.intervalAsk)
        log('#onBestPriceChanged() price:' + price)

        if (!checkPrice(price, tradeRule))  return
        if (!checkBalance(quoteReq.quantity * price, product())) return // check balance

        sendPrice(price)
    }

    onIndicBidChanged: {
        if (quoteReq.isBuy)  return

        var tradeRule = tradeRules[security]
        log('#onIndicBidChanged() tradeRule: ' + tradeRule + ' for security: ' + security)
        if (!tradeRule) return // this script does not setted up for such product

        if (initialIndicBid === 0) {
            initialIndicBid = indicBid
            log('#onIndicBidChanged() hold up initialIndicBid value:' + initialIndicBid)
        }

        var price = 0
        if (prevSendPrice === 0) {
            price = indicBid * tradeRule.max // always starts from [,max] level
        } else if ((indicBid - prevSendPrice) < (indicBid * tradeRule.intervalBid)) {
            price = indicBid * (1.00 - tradeRule.intervalBid)
        }
        log('#onIndicBidChanged() price:' + price)

        if (!checkPrice(price, tradeRule)) {
            return
        }
        if (!checkBalance(quoteReq.quantity * price, product())) {
            return
        }
        sendPrice(price)
    }

    onIndicAskChanged: {
        if (!quoteReq.isBuy) return

        var tradeRule = tradeRules[security]
        log('#onIndicAskChanged() tradeRule: ' + tradeRule + ' for security: ' + security)
        if (!tradeRule) return // this script does not setted up for such product

        if (initialIndicAsk === 0) {
            initialIndicAsk = indicAsk
            log('#onIndicAskChanged() hold up initialIndicAsk value:' + initialIndicBid)
        }

        var price = 0
        if (prevSendPrice === 0) {
            price = indicBid * tradeRule.max // always starts from [,max] level
        } else if ((prevSendPrice - indicAsk) < (indicAsk * tradeRule.intervalAsk)) {
            price = indicAsk * (1.00 + tradeRule.intervalAsk)
        }
        log('#onIndicAskChanged() price:' + price)

        if (!checkPrice(price, tradeRule))  return
        if (!checkBalance(quoteReq.quantity, quoteReq.product))  return

        sendPrice(price)
    }

    onSendFailed: {  // Invoked when sending of reply has failed
        log("Sending failed: " + reason)
    }
}
