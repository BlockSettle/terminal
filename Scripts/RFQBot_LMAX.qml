/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import bs.terminal 1.0
BSQuoteReqReply {
    //------------------------INTERFACE---------------------------
    property var firstShiftFromIndicPrice: 2 // in percent       -
    property var shiftLimitFromIndicPrice: 0.2 // in percent     -
    //------------------------------------------------------------


    //-----------------------BRIEF EXPLANATION-------------------
    /*
      On the first call of either onIndicBidChanged or onIndicBidChanged the script counts the initialPrice, check the limits and sends it to server on succeed.
      Notice,  that script won't react on bid or ask changing after that.
      Next, on each onBestPriceChanged call script counts new price depening on direction() call that also counts quote or base currency was used.
      If the price satisfies the limits - it's sent to server.
      checkPrice() - checks whether the price lays between user limits
      checkBalance() - checks whether the user have enough balance for the quantity of products.
    */

    property var initialPrice: 0.0
    property var prcIncrement: 0.005 // in percent
    property var priceIncrement: 0.01
    property var finalPrice: 0.0
    property var hedgePrice: 0.0
    property var hedgeAllowed: true
    property var settled: false

    onStarted: {    // serve only fixed CC quotes here
        if (!isCC()) {
            var subs = '{"symbol":"' + security + '"}'
            sendExtConn('LMAX', 'subscribe_prices', subs)
            return
        }
        if (direction() !== 1)  return
        if (security == "LOL/XBT") {
            sendPrice(0.0001)
        }
        else if (security == "ARM/XBT") {
            sendPrice(0.001)
        }
    }

    onCancelled: {
	settled = true
        //todo: stop
    }

    onBestPriceChanged: {
        if (!hedgeAllowed || settled) return
        log('new best price: ' + bestPrice)
        if (bestPrice === finalPrice)  return   // our quote

        if (hedgePrice) {
            if (direction() === 1) {
                if (bestPrice > hedgePrice) {
                    sendPrice(hedgePrice)
                }
                else {
                    log("give up - best price too low: " + bestPrice + " vs " + hedgePrice)
                }
            }
            else {
                if (bestPrice < hedgePrice) {
                    sendPrice(hedgePrice)
                }
                else {
                    log("give up - best price too high: " + bestPrice + " vs " + hedgePrice)
                }
            }
        }
    }

    onExtDataReceived: {
//        log('from: ' + from + ', type: ' + type + ', msg:\n' + msg)
        if (from === 'LMAX') {
            var msgObj = JSON.parse(msg)
            if (type === 'mdprices') {
                onLMAXprices(msgObj)
            }
            else if (type === 'order_intent') {
                onLMAXintent(msgObj)
            }
        }
    }
    
    onSettled: {
        settled = true
        log(security + ' settled at ' + finalPrice)
        sendHedgeOrder(hedgePrice)

        var subs = '{"symbol":"' + security + '"}'
        sendExtConn('LMAX', 'unsubscribe_prices', subs)
    }

    function onLMAXprices(msgObj)
    {
        for (var i in msgObj.prices) {
            var priceObj = msgObj.prices[i]
            if (priceObj.symbol === security) {
                onLMAXprice(priceObj)
            }
        }
    }

    function onLMAXprice(priceObj)
    {
        if (!hedgeAllowed) return
        if (direction() === 1) {
            hedgePrice = priceObj.ask
        }
        else {
            hedgePrice = priceObj.bid
        }

        if (settled) {
            return
        }

        var price = 0.0
        if (direction() === 1) {
            price = priceObj.ask
        }
        else {
            price = priceObj.bid
        }

        if (bestPrice) {
            if (bestPrice != finalPrice) {   // at least one quote exists
                if (direction() === 1) {     // and it's not ours
                    if ((bestPrice < hedgePrice) && (finalPrice < bestPrice)) {
                        price = Math.min(bestPrice + priceIncrement, hedgePrice)
                    }
                }
                else {
                    if ((bestPrice > hedgePrice) && (finalPrice > bestPrice)) {
                        price = Math.max(hedgePrice, bestPrice - priceIncrement)
                    }
                }
                if (price) {
                    log("beating competitor")
                    sendPrice(price)
                }
            }
            else {
                if (direction() === 1) {
                    if (finalPrice < price) {
                        log("improving own bid")
                        sendPrice(price)
                    }
                }
                else {
                    if (finalPrice > price) {
                        log("improving own ask")
                        sendPrice(price)
                    }
                }
            }
        }
        else {  // no quote was sent before
            log("sending first RFQ response")
            sendPrice(price)
        }
    }

    function onLMAXintent(intentObj)
    {
        if ((intentObj.symbol !== security) || (intentObj.buy !== (direction() === 1))) {
            return  // not our intent
        }
        hedgeAllowed = intentObj.allowed
        if (!intentObj.allowed) {
            log('intent is not allowed - pulling reply')
            pullQuoteReply()
            initialPrice = 0
            finalPrice = 0
        }
    }

    function isContraCur()
    {
        if( typeof isContraCur.value == 'undefined' ) {
            isContraCur.value = (security.split("/")[1] === quoteReq.product);
        }
        return isContraCur.value;
    }

    function contraCur(product)
    {
        var spl = security.split("/")
        return (spl[1] === product) ? spl[0] : spl[1];
    }

    function checkBalance(price)
    {
        var value = quoteReq.quantity
        var product = quoteReq.product
        if (!quoteReq.isBuy) {
            product = contraCur(quoteReq.product)
            if (isContraCur()) {
                value = quoteReq.quantity / price
            }
            else {
                value = quoteReq.quantity * price
            }
        }

        var balance = accountBalance(product)
        if (value > balance) {
            log("Not enough balance for " + product + ": " + value + " > " + balance)
            return false
        }
        return true
    }

    function sendPrice(price)
    {
        if (finalPrice === price) {
            log('price ' + price + ' not changed')
            return
        }
        finalPrice = price
        if (!initialPrice) {
            initialPrice = price
        }

        if (!checkBalance(price)) return

        log('sending new reply: ' + price)
        sendQuoteReply(price)
        sendOrderIntent(hedgePrice)
    }

    function hedgeOrderBuy()
    {
        var orderBuy = (direction() === 1) ? 'true' : 'false'
        return orderBuy
    }

    function hedgeOrderAmount(price)
    {
        var amount = isContraCur() ? quoteReq.quantity / price : quoteReq.quantity;
        return amount
    }

    function isCC()
    {
        return (quoteReq.assetType == 3)
    }

    function isXBT()
    {
        return (quoteReq.assetType == 2)
    }

    function sendHedgeOrder(price)
    {
        if (!price) {
            log('sendHedgeOrder: invalid zero price');
            return
        }

        if (isCC()) {
            return
        }
        if (isXBT()) {
            if (hedgeOrderAmount(price) > 1.0) {
                log('XBT amount exceeds limit: ' + hedgeOrderAmount(price))
                return
            }
        }
        var order = '{"symbol":"' + security + '", "buy":' + hedgeOrderBuy()
            + ', "amount":' + hedgeOrderAmount(price) + ', "price":' + price + '}'
        log('sending order: ' + order)
        sendExtConn('LMAX', 'order', order)
    }

    function sendOrderIntent(price)
    {   // This request just signals LMAX connector that order will follow soon,
        // if LMAX's reply on it is negative, quote reply should be pulled
        // price is used here only to calculate contra qty
        if (isCC() || settled) return
        if (!price) {
            return
        }
        var intent = '{"symbol":"' + security + '", "buy":' + hedgeOrderBuy()
            + ', "amount":' + hedgeOrderAmount(price) + '}'
        sendExtConn('LMAX', 'order_intent', intent)
    }

    function direction()
    {
        if( typeof direction.value == 'undefined' ) {
            if ((quoteReq.isBuy && !isContraCur()) || (!quoteReq.isBuy && isContraCur())) {
                direction.value = 1;
            }
            else {
                direction.value = -1;
            }
        }
        return direction.value;
    }
}
