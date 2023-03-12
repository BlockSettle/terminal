/*

***********************************************************************************
* Copyright (C) 2018 - 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.9
import QtQuick.Controls 2.3

import "../BsStyles"
import "../StyledControls"

Item {
    id: root

    property int time_progress

    property alias value: password.input_text

    CustomTextInput {
        id: password

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 0

        width: root.width
        height: 70

        input_topMargin: 35
        title_leftMargin: 16
        title_topMargin: 16

        visible: txSignRequest !== null ? !txSignRequest.isHWW : false
        title_text: qsTr("Password")

        isPassword: true
        isHiddenText: true
    }

    CustomProgressBar {
        id: progress_bar

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: password.bottom
        anchors.topMargin: 16

        width: root.width

        from: 0
        to: 120

        value: root.time_progress
    }

    Label {
        id: progress_label

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: progress_bar.bottom
        anchors.topMargin: 8

        text: qsTr("%1 seconds left").arg (Number(root.time_progress).toLocaleString())

        color: "#45A6FF"

        font.pixelSize: 13
        font.family: "Roboto"
        font.weight: Font.Normal
    }

    function setActiveFocus() {
        password.setActiveFocus()
    }

}
