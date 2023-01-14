import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15

import "../BsStyles"
import "../StyledControls"
ColumnLayout  {

    id: layout

    property var phrase
    signal sig_verified()
    signal sig_skipped()

    property var indexes: []

    height: 481
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

        delegate: CustomSeedTextInput {
            id: _delegate

            width: 530
            title_text: layout.indexes[index]
            isValid: list.isValid
            onTextChanged : {
                list.isComplete = true
                for (var i = 0; i < list.count; i++)
                {
                    if(list.itemAtIndex(i).input_text === "")
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

        //Layout.leftMargin: 24
        Layout.bottomMargin: 40
        Layout.alignment: Qt.AlignBottom | Qt.AlignHCenter

        CustomButton {
            id: skip_but
            text: qsTr("Skip")
            width: 261

            Component.onCompleted: {
                skip_but.preferred = false
            }

            function click_enter() {
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

            function click_enter() {
                if (!continue_but.enabled) return

                list.isValid = true
                for (var i = 0; i < list.count; i++)
                {
                    if(list.itemAtIndex(i).input_text !== layout.phrase[layout.indexes[i] - 1])
                    {
                        list.isValid = false
                        break
                    }
                }
                if (list.isValid)
                {
                    layout.sig_verified()
                }
            }

        }

   }

   function createRandomIndexes() {
        var idx = []
        while(idx.length < 4)
        {
            var r = Math.floor(Math.random() * 12) + 1;
            if(idx.indexOf(r) === -1) idx.push(r);
        }
        for(var i_ord = 0; i_ord < 4; i_ord++)
        {
            for(var i = 0; i < 3 - i_ord; i++)
            {
                if(idx[i] > idx[i+1])
                {
                    var temp = idx[i]
                    idx[i] = idx[i+1]
                    idx[i+1] = temp
                }
            }
        }
        layout.indexes = idx
        list.model = idx
   }

   function init()
   {
       createRandomIndexes()
       list.itemAtIndex(0).setActiveFocus()
   }
}

