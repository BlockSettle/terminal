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
    property var applyScale
    property var applyWindowWidthScale
    property var applyWindowHeightScale

    function setupScaleFunctions() {
        applyScale = function (size) {
            return size * scaleController.scaleRatio;
        }
        applyWindowWidthScale = function (size) {
            return Math.min(applyScale(size), scaleController.screenWidth)
        }
        applyWindowHeightScale = function (size) {
           return Math.min(applyScale(size), (scaleController.screenHeight - applyScale(100)))
            }
    }

    Connections {
        target: scaleController
        function onChanged() {
            setupScaleFunctions()
        }
    }

    Component.onCompleted: setupScaleFunctions()
}
