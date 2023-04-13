/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
pragma Singleton
import QtQuick 2.0


Item {
    function applyScale(size) {
        return size * scaleController.scaleRatio;
    }

    function applyWindowWidthScale(size) {
        return Math.min(applyScale(size), scaleController.screenWidth)
    }

    function applyWindowHeightScale(size) {
        return Math.min(applyScale(size), (scaleController.screenHeight - applyScale(100)))
    }
}
