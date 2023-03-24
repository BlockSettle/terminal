/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.15

import "../Styles"

Column {
    property string header_text
    property string currency
    property string currency_icon
    property string comment

    spacing: 10

    Text {
        text: header_text
        font.family: "Roboto"
        font.pixelSize: 14
        color: SideSwapStyles.secondaryTextColor
    }

    Row {
        spacing: 10

        Image {
            source: currency_icon
            width: 24
            height: 24
        }

        Text {
            text: currency
            font.family: "Roboto"
            font.pixelSize: 18
            color: SideSwapStyles.primaryTextColor
        }
    }


    Text {
        text: comment
        font.family: "Roboto"
        font.pixelSize: 16
        color: SideSwapStyles.parapraphTextColor
    }
}

