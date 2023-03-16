import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15

import "../BsStyles"
import "../StyledControls"

ColumnLayout  {

    id: layout

    signal sig_continue()

    height: 485
    width: 580

    spacing: 0

    Component.onCompleted: {
        var xhr = new XMLHttpRequest;
        xhr.open("GET", "qrc:/TermsAndConditions.txt");
        xhr.onreadystatechange = function() {
            if (xhr.readyState === XMLHttpRequest.DONE) {
                edit.text = xhr.responseText;
            }
        };
        xhr.send();
    }

    CustomTitleLabel {
        id: title
        Layout.alignment: Qt.AlignLeft
        Layout.leftMargin: 24
        Layout.preferredHeight : title.height
        text: qsTr("Terms and conditions")
    }

    Label {
        Layout.fillWidth: true
        height: 24
    }

    ScrollView {
        id: scroll
        Layout.alignment: Qt.AlignLeft
        Layout.leftMargin: 24
        implicitWidth: 532
        implicitHeight: 340

        ScrollBar.vertical.policy: ScrollBar.AlwaysOn

        clip: true

        TextArea {
            id: edit

            width: parent.width
            height: parent.height
            font.pixelSize: 14
            font.family: "Roboto"
            font.weight: Font.Normal
            leftPadding: 0
            rightPadding: 10
            color: BSStyle.titanWhiteColor
            selectByMouse: true
            wrapMode: TextEdit.WordWrap
            readOnly: true

            background: Rectangle {
                color: "transparent"
                radius: 4
            }

        }
    }

    Rectangle {
        Layout.fillWidth: true
        height: 1
        color: BSStyle.defaultGreyColor
    }

    Label {
        Layout.fillWidth: true
        height: 24
    }

    CustomButton {
        id: continue_but
        text: qsTr("Continue")
        Layout.leftMargin: 24
        width: 532
        preferred: true

        function click_enter() {
            bsApp.settingActivated = true
            sig_continue()
        }
    }

    Label {
        Layout.fillWidth: true
        height: 24
    }


    Keys.onEnterPressed: {
        continue_but.click_enter()
    }

    Keys.onReturnPressed: {
        continue_but.click_enter()
    }
}

