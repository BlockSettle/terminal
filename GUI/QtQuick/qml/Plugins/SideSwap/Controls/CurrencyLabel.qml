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
import "../../../BsStyles"

Column {
    property string header_text
    property string currency
    property string currency_icon
    property string comment

    spacing: BSSizes.applyScale(10)

    Text {
        text: header_text
        font.family: "Roboto"
        font.pixelSize: BSSizes.applyScale(14)
        color: SideSwapStyles.secondaryTextColor
    }

    Row {
        spacing: BSSizes.applyScale(10)

        Image {
            source: currency_icon
            width: BSSizes.applyScale(24)
            height: BSSizes.applyScale(24)
        }

        Text {
            text: currency
            font.family: "Roboto"
            font.pixelSize: BSSizes.applyScale(18)
            color: SideSwapStyles.primaryTextColor
        }
    }


    Text {
        text: comment
        font.family: "Roboto"
        font.pixelSize: BSSizes.applyScale(16)
        color: SideSwapStyles.paragraphTextColor
    }
}

