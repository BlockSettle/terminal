import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15

import "../BsStyles"
import "../StyledControls"

ColumnLayout  {

    id: layout

    signal sig_continue()

    height: BSSizes.applyScale(485)
    width: BSSizes.applyScale(580)

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
        Layout.leftMargin: BSSizes.applyScale(24)
        Layout.preferredHeight : title.height
        text: qsTr("Terms and conditions")
    }

    Label {
        Layout.fillWidth: true
        height: BSSizes.applyScale(24)
    }

    ScrollView {
        id: scroll
        Layout.alignment: Qt.AlignLeft
        Layout.leftMargin: BSSizes.applyScale(24)
        implicitWidth: BSSizes.applyScale(532)
        implicitHeight: BSSizes.applyScale(340)

        ScrollBar.vertical.policy: ScrollBar.AlwaysOn

        clip: true

        TextArea {
            id: edit

            width: parent.width
            height: parent.height
            font.pixelSize: BSSizes.applyScale(14)
            font.family: "Roboto"
            font.weight: Font.Normal
            leftPadding: 0
            rightPadding: BSSizes.applyScale(10)
            color: BSStyle.titanWhiteColor
            selectByMouse: true
            wrapMode: TextEdit.WordWrap
            readOnly: true

            background: Rectangle {
                color: "transparent"
                radius: BSSizes.applyScale(4)
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
        height: BSSizes.applyScale(24)
    }

    CustomButton {
        id: continue_but
        text: qsTr("Continue")
        Layout.leftMargin: BSSizes.applyScale(24)
        width: BSSizes.applyScale(532)
        preferred: true

        function click_enter() {
            bsApp.settingActivated = true
            sig_continue()
        }
    }

    Label {
        Layout.fillWidth: true
        height: BSSizes.applyScale(24)
    }


    Keys.onEnterPressed: {
        continue_but.click_enter()
    }

    Keys.onReturnPressed: {
        continue_but.click_enter()
    }
}

