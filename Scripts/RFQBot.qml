/*

***********************************************************************************
* Copyright (C) 2016 - , BlockSettle AB
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
    property var directionInvoked: false
    property var directionSign: 1
    property var priceIncrement: 0.0001 // in percent

    property var tradeRules: [
        'EUR/GBP',
        'EUR/JPY',
        'EUR/SEK',
        'GBP/JPY',
        'GBP/SEK',
        'JPY/SEK',
    ]


    onBestPriceChanged: {
        if (tradeRules.indexOf(security) === -1) return;
        var price = addIncrementToFXPrice(bestPrice);

        if (!checkPrice(price))  return
        if (!checkBalance(quoteReq.quantity * price, security.split("/")[1])) return

        sendPrice(price)
    }
    onExpirationInSecChanged: {
        if (tradeRules.indexOf(security) === -1) return;
        if (initialPrice) return;
        var price = countInitialPrice();

        if (!checkPrice(price))  return
        if (!checkBalance(quoteReq.quantity * price, security.split("/")[1])) return // check balance
        log('#set first price: ' + price)
        sendPrice(price)

    }
    onIndicBidChanged: {
        log('#new indic bid: ' + indicBid)
        onBidOrAskChanged();
    }
    onIndicAskChanged: {
        log('#new indic ask: ' + indicAsk)
        onBidOrAskChanged();
    }
    onSendFailed: {

    }

    function checkPrice(price) {
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

    function isContraCur(){
        if( typeof isContraCur.value == 'undefined' ) {
            isContraCur.value = (security.split("/")[1] === quoteReq.product);
        }
        return isContraCur.value;
    }

    function onBidOrAskChanged(){
        log ("security: " + security)
        if (tradeRules.indexOf(security) === -1) return;
        if (initialPrice) return;
        log ("direction: " + direction())
        var price =  countInitialPrice();

        log ("set new price: "+ price)
        if (!checkPrice(price))  return
        log("price is OK");
        if (!checkBalance(quoteReq.quantity * price, security.split("/")[1])) return // check balance
        log("balance is OK");
        sendPrice(price)
    }

    function checkBalance(value, product) {
        var balance = accountBalance(product)
        if (value > balance) {
            log("Not enough balance for " + product + ": " + value + " > " + balance)
            return false
        }
        return true
    }

    function countInitialPrice(){
        var indicChoose = 0;
        if (quoteReq.isBuy){
            if (!isContraCur()){
                indicChoose = indicAsk ;
            } else {
                indicChoose = indicBid ;
            }
        } else {
            if (!isContraCur()){
                indicChoose =  indicBid;
            } else {
                indicChoose = indicAsk;
            }
        }

        log("count initital price: " + indicChoose * ((100.00 + firstShiftFromIndicPrice * direction()) / 100.00));
        return indicChoose * ((100.00 + firstShiftFromIndicPrice * direction()) / 100.00);
    }

    function sendPrice(price) {
        if (!initialPrice){
            initialPrice = price;
        }

        sendQuoteReply(price)
    }

    function addIncrementToFXPrice(price){

        const baseIncrement = 0.0001;
        const incrementInPercent = 0.01;

        const coeff = incrementInPercent / 100.0;
        if (indicAsk  * (coeff + 1) > indicAsk + baseIncrement ){
            return price * (1 + coeff * direction() * -1);
        } else {
            return price + baseIncrement * direction() * -1;
        }
    }

    function direction() {
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
