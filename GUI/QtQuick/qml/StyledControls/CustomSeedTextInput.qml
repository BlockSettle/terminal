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
import "../BsStyles"

CustomTextInput {
    height: BSSizes.applyScale(46)
    horizontalAlignment : TextInput.AlignHCenter
    input_topMargin: BSSizes.applyScale(13)
    title_leftMargin: BSSizes.applyScale(10)
    title_topMargin: BSSizes.applyScale(8)
    title_font_size: BSSizes.applyScale(12)
}
