/*

***********************************************************************************
* Copyright (C) 2018 - 2022, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.9
import QtQuick.Controls 2.3
import "../BsStyles"

Label {
    id: title
    height : BSSizes.applyScale(23)
    color: BSStyle.titanWhiteColor
    font.pixelSize: BSSizes.applyScale(20)
    font.family: "Roboto"
    font.weight: Font.Medium
}
