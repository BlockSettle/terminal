import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15

import "StyledControls"
import "BsStyles"


ColumnLayout  {

    id: layout

    property var phrase
    signal sig_verified()
    signal sig_skipped()

    property var indexes: [  "1",  "6",  "8", "11"]

    height: 521
    width: 580

    spacing: 0

    CustomTitleLabel {
        id: title
        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : title.height
        text: "Verify your seed"
    }

    ListView {
        id: list

        Layout.fillHeight: true
        Layout.fillWidth: true
        Layout.leftMargin: 25
        Layout.topMargin: 32

        spacing  : 10
        property var isValid: true
        property var isComplete: false

        model: indexes
        delegate: CustomSeedTextInput {
            width: 530
            serial_num: layout.indexes[index]
            isValid: list.isValid
            onTextChanged : {
                list.isComplete = true
                for (var i = 0; i < list.count; i++)
                {
                    if(list.itemAtIndex(i).seed_text === "")
                    {
                        list.isComplete = false
                        break
                    }
                }
            }
        }

    }

    Label {
        id: error_description

        visible: !list.isValid

        text: "Your words are wrong"

        Layout.leftMargin: 222
        Layout.bottomMargin: 114
        Layout.preferredHeight : 16

        height: 16
        width: 136

        color: "#EB6060"
        font.pixelSize: 14
        font.family: "Roboto"
        font.weight: Font.Normal
    }

    RowLayout {
        id: row
        spacing: 10

        Layout.leftMargin: 24
        Layout.bottomMargin: 40

        CustomButton {
            id: skip_but
            text: qsTr("Skip")
            width: 261

            Component.onCompleted: {
                skip_but.preferred = false
            }

            onClicked: {
                layout.sig_skipped()
            }
        }

        CustomButton {
            id: continue_but
            text: qsTr("Continue")
            width: 261
            enabled: list.isComplete

            Component.onCompleted: {
                continue_but.preferred = true
            }

            onClicked: {
                list.isValid = true
                for (var i = 0; i < list.count; i++)
                {
                    console.log("layout.indexes[i] - " + layout.indexes[i])
                    console.log("layout.phrase[layout.indexes[i]]) - " + layout.phrase[layout.indexes[i]] - 1)
                    console.log("list.itemAtIndex(i).seed_text - " + list.itemAtIndex(i).seed_text)
                    if(list.itemAtIndex(i).seed_text !== layout.phrase[layout.indexes[i] - 1])
                    {
                        list.isValid = false
                        break
                    }
                }
                if (list.isValid)
                {
                    console.log("layout.verified")
                    layout.sig_verified()
                }
            }

        }

   }
}

