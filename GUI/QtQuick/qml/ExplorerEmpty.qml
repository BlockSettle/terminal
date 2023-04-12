/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtQml.Models 2

import "BsStyles"

Item {
    anchors.fill: parent
    anchors.topMargin: BSSizes.applyScale(226)

    Column {
        spacing: BSSizes.applyScale(24)
        anchors.horizontalCenter: parent.horizontalCenter

        Image {
            width: BSSizes.applyScale(57)
            height: BSSizes.applyScale(72)
            source: "qrc:/images/logo_no_text.svg"
            anchors.horizontalCenter: parent.horizontalCenter
        }
        Label {
            text: qsTr("Provides you with a convenient, powerful, yet simple tool to read\n transaction and address data from the bitcoin network")
            font.pixelSize: BSSizes.applyScale(16)
            color: BSStyle.titleTextColor
            horizontalAlignment: Text.AlignHCenter
        }
    }
}
