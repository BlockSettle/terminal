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

    height: BSSizes.applyScale(485)
    width: BSSizes.applyScale(580)

    spacing: 0

    CustomTitleLabel {
        id: title
        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : title.height
        text: qsTr("Verify your seed")
    }

    ListView {
        id: list

        Layout.fillHeight: true
        Layout.fillWidth: true
        Layout.leftMargin: BSSizes.applyScale(25)
        Layout.rightMargin: BSSizes.applyScale(25)
        Layout.topMargin: BSSizes.applyScale(32)

        spacing: BSSizes.applyScale(10)
        property var isValid: true
        property var isComplete: false

        delegate: CustomSeedTextInput {
            id: _delegate

            width: parent.width
            title_text: layout.indexes[index]
            isValid: list.isValid
            onTextEdited : {
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
            onEnterPressed: {
                continue_but.click_enter()
            }
            onReturnPressed: {
                continue_but.click_enter()
            }
        }

    }

    Label {
        id: error_description

        visible: !list.isValid

        text: qsTr("Your words are wrong")

        Layout.leftMargin: BSSizes.applyScale(222)
        Layout.bottomMargin: BSSizes.applyScale(114)
        Layout.preferredHeight: BSSizes.applyScale(16)

        height: BSSizes.applyScale(16)
        width: BSSizes.applyScale(136)

        color: "#EB6060"
        font.pixelSize: BSSizes.applyScale(14)
        font.family: "Roboto"
        font.weight: Font.Normal
    }

    RowLayout {
        id: row
        spacing: BSSizes.applyScale(10)

        //Layout.leftMargin: 24
        Layout.bottomMargin: BSSizes.applyScale(40)
        Layout.alignment: Qt.AlignBottom | Qt.AlignHCenter

        CustomButton {
            id: skip_but
            text: qsTr("Skip")
            width: BSSizes.applyScale(261)

            preferred: false

            function click_enter() {
                layout.sig_skipped()
            }
        }

        CustomButton {
            id: continue_but
            text: qsTr("Continue")
            width: BSSizes.applyScale(261)

            enabled: list.isComplete

            preferred: true

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

