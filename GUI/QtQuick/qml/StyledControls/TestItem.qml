/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.9
import QtQuick.Controls 2.3

Rectangle {
    width: 300
    height: 300
    color: "black"

    FontLoader {
        id: robotoFont
        source: "file:////home/yauhen/Workspace/terminal/GUI/QtQuick/fonts/Roboto-Regular.ttf"
    }

    FontLoader {
        id: robotoFontBold
        source: "file:////home/yauhen/Downloads/Roboto/Roboto-Bold.ttf"
    }

    Column {
        anchors.fill: parent
        spacing: 1

        Text {
            text: "Confirmed balance"
            font.pixelSize: 12
            font.family: "Roboto"
            color: "red"
            font.letterSpacing: -0.2
        }

        Text {
            text: "0.00004494 BTC"
            font.pixelSize: 14
            font.family: "Roboto"
            font.weight: Font.Bold
            font.letterSpacing: 0.2
            color: "red"
        }

        Text {
            text: "0.00004495 BTC"
            font.pixelSize: 14
            font.family: 'Helvetica'
            font.weight: Font.Bold
            font.letterSpacing: 0.2
            color: "red"
        }

        Text {
            text: "Addresses"
            font.pixelSize: 20
            font.family: "Roboto"
            font.weight: Font.Bold
            font.letterSpacing: 0.35
            color: "red"
        }

        Text {
            text: "Alex Wallet"
            font.pixelSize: 16
            font.family: "Roboto"
            color: "red"
        }

        Text {
            text: "Wallet Properties"
            font.pixelSize: 12
            font.family: "Roboto"
            font.letterSpacing: 0.3
            color: "red"
        }

        Text {
            text: "Incoming transactions"
            font.pixelSize: 16
            font.family: "Roboto"
            font.weight: Font.DemiBold
            color: "red"
        }

        Text {
            text: "tb1qfxa27xqumquwcstuejvswv2tqt78z9a8r9dy7d"
            font.pixelSize: 13
            font.family: "Roboto"
            color: "red"
            font.letterSpacing: -0.2
        }

        Text {
            text: "Transactions"
            font.pixelSize: 14
            font.family: "Roboto"
            color: "red"
        }
    }
}