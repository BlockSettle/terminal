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

    property var ccInstruments: [
        'LOL/XBT',
        'POC/XBT'
    ]
    property var xbtInstruments: [
        'XBT/CAD',
        'XBT/EUR',
        'XBT/GBP',
        'XBT/PLN'
    ]


    onBestPriceChanged: {
        log('new best price: ' + bestPrice)
        if (bestPrice === finalPrice)  return   // our quote

        if (hedgePrice) {
            if (quoteReq.isBuy) {
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

/*        var price = calcInitialPrice();

        if (!checkPrice(price))  return
        if (!checkBalance(quoteReq.quantity * price, security.split("/")[1])) return

        sendPrice(price)*/
    }

    onExpirationInSecChanged: {
/*        if (initialPrice) return;
        var price = countInitialPrice();

        if (!checkPrice(price))  return
        if (!checkBalance(quoteReq.quantity * price, security.split("/")[1])) return // check balance
        log('#set first price: ' + price)
        sendPrice(price)*/
    }
    onIndicBidChanged: {
        onBidOrAskChanged();
    }
    onIndicAskChanged: {
        onBidOrAskChanged();
    }

    onExtDataReceived: {
//        log('from: ' + from + ', type: ' + type + ', msg:\n' + msg)
        if ((from === 'LMAX') && (type === 'mdprices')) {
            var msgObj = JSON.parse(msg)
            onLMAXprices(msgObj)
        }
    }
    
    onSettled: {
        if (!(ccInstruments.indexOf(security) === -1)) return;
        log(security + ' settled at ' + finalPrice)
        sendHedgeOrder(hedgePrice)
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
        if (quoteReq.isBuy) {
            hedgePrice = priceObj.ask * (1.0 + prcIncrement)
        }
        else {
            hedgePrice = priceObj.bid * (1.0 - prcIncrement)
        }

        var price = 0.0
        if (bestPrice && (bestPrice != finalPrice)) {   // at least one quote exists
            if (quoteReq.isBuy) {                       // and it's not ours
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
        else if (bestPrice && (bestPrice === finalPrice)) {}    // do nothing
        else {  // no quote was sent before
            if (quoteReq.isBuy) {
                price = priceObj.ask * (1.0 + 3 * prcIncrement)
            }
            else {
                price = priceObj.bid * (1.0 - 3 * prcIncrement)
            }
            log("sending first RFQ response")
            sendPrice(price)
        }
    }

    function checkPrice(price)
    {
        if (initialPrice){
            if (!isContraCur()){
                if (quoteReq.isBuy && price < indicAsk * ((100.00 + shiftLimitFromIndicPrice * direction()) / 100.00)) return false;
                if (!quoteReq.isBuy && price > indicBid * ((100.00 + shiftLimitFromIndicPrice * direction()) / 100.00)) return false;
            } else {
                if (quoteReq.isBuy && price > indicBid * ((100.00 + shiftLimitFromIndicPrice * direction()) / 100.00)) return false;
                if (!quoteReq.isBuy && price < indicAsk * ((100.00 + shiftLimitFromIndicPrice * direction()) / 100.00)) return false;
            }

        }


        if (!price || price < 0) return false
        return true
    }

    function isContraCur()
    {
        if( typeof isContraCur.value == 'undefined' ) {
            isContraCur.value = (security.split("/")[1] === quoteReq.product);
        }
        return isContraCur.value;
    }

    function onBidOrAskChanged()
    {
/*        log ("security: " + security)
        if (initialPrice) return;
        log ("direction: " + direction())
        var price = countInitialPrice();
        log ("set new price: "+ price)
        if (!checkPrice(price))  return
        log("price is OK");
        if (!checkBalance(quoteReq.quantity * price, security.split("/")[1])) return // check balance
        log("balance is OK");
        sendPrice(price)*/
    }

    function checkBalance(value, product)
    {
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

        if (quoteReq.isBuy) {
            if (!checkBalance(quoteReq.quantity * price, security.split("/")[1])) return
        }
        else {
            if (!checkBalance(quoteReq.quantity, security.split("/")[0])) return
        }

        log('sending new reply: ' + price)
        sendQuoteReply(price)
    }

    function sendHedgeOrder(price)
    {
        if (ccInstruments.indexOf(security) != -1) return
        if (xbtInstruments.indexOf(security) != -1) {
            if ((quoteReq.quantity > 1.0) || (quoteReq.quantity < 0.01)) {
                log('XBT amount is out of limits: ' + quoteReq.quantity)
                return
            }
        }
        else {
            if (quoteReq.quantity < 1000) {
                log('FX amount is too low - not hedging it atm')
                return
            }
        }

        var orderBuy = quoteReq.isBuy ? 'true' : 'false'
        var order = '{"symbol":"' + security + '", "buy":' + orderBuy
            + ', "amount":' + quoteReq.quantity + ', "price":' + price + '}'
        log('sending order: ' + order)
        sendExtConn('LMAX', 'order', order)
    }

    function addIncrementToFXPrice(price)
    {
        const baseIncrement = 0.0001;
        const incrementInPercent = 0.01;

        const coeff = incrementInPercent / 100.0;
        if (indicAsk  * (coeff + 1) > indicAsk + baseIncrement ){
            return price * (1 + coeff * direction() * -1);
        } else {
            return price + baseIncrement * direction() * -1;
        }
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
