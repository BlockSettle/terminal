/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.12

Loader {
    id: page

    function loadTX(tx) {
        page.source = ""
        page.source = Qt.resolvedUrl("ExplorerTX.qml")
        page.item.tx = tx
    }
}
