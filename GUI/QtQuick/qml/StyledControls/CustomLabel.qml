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

Label {
    horizontalAlignment: Text.AlignHLeft
    font.pixelSize: 11
    color: { enabled ? BSStyle.labelsTextColor : BSStyle.disabledColor }
    wrapMode: Text.WordWrap
    topPadding: 5
    bottomPadding: 5
}

