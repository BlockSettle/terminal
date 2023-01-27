import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15

import "../BsStyles"
import "../StyledControls"


ColumnLayout  {

    id: layout

    signal sig_import()
    signal sig_only_watching()

    property bool isFileChoosen: false
    property var phrase: []

    property var grid_model_12: ["1", "2", "3", "4",
                                 "5", "6", "7", "8",
                                 "9", "10", "11", "12"]
    property var grid_model_24: ["1", "2", "3", "4",
                                 "5", "6", "7", "8",
                                 "9", "10", "11", "12",
                                 "13", "14", "15", "16",
                                 "17", "18", "19", "20",
                                 "21", "22", "23", "24"]

    height: radbut_12.checked ? 515 : 739
    width: 580

    spacing: 0

    CustomTitleLabel {
        id: title
        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : title.height
        text: qsTr("Import wallet")
    }

    CustomTextSwitch {
        id: type_switch

        Layout.alignment: Qt.AlignCenter
        Layout.topMargin: 24

        onSig_full_changed: (isFull) => {
            if (isFull === false)
            {
                type_switch.isFullChoosed = true
                layout.sig_only_watching()
            }
        }

        Component.onCompleted: {
            type_switch.isFullChoosed = true
        }
    }

    RowLayout {
        id: row
        spacing: 12

        Layout.alignment: Qt.AlignCenter
        Layout.topMargin: 32
        Layout.preferredHeight: 19

        Label {
            Layout.fillWidth: true
        }

        Label {
            id: radbut_text

            text: "Seed phrase type:"

            Layout.leftMargin: 25

            width: 126
            height: 19

            color: "#E2E7FF"
            font.pixelSize: 16
            font.family: "Roboto"
            font.weight: Font.Normal
        }

        CustomRadioButton {
            id: radbut_12

            text: "12 words"

            spacing: 6
            font.pixelSize: 13
            font.family: "Roboto"
            font.weight: Font.Normal

            checked: true
        }

        CustomRadioButton {
            id: radbut_24

            text: "24 words"

            spacing: 6
            font.pixelSize: 13
            font.family: "Roboto"
            font.weight: Font.Normal

            checked: false
        }

        Label {
            Layout.fillWidth: true
        }
    }

    Rectangle {
        id: hor_line

        Layout.topMargin: 16
        Layout.fillWidth: true

        height: 1

        color: "#3C435A"
    }

    GridView {
        id: grid

        Layout.fillHeight: true
        Layout.fillWidth: true
        Layout.leftMargin: 25
        Layout.topMargin: 24

        cellHeight : 56
        cellWidth : 180

        property bool isComplete: false

        model: radbut_12.checked ? layout.grid_model_12 : layout.grid_model_24
        delegate: CustomSeedTextInput {
            id: _delega

            property bool isAccepted: false

            width: 170
            title_text: modelData
            onTextChanged : {
                grid.isComplete = true

                for (var i = 0; i < grid.count; i++)
                {
                    if(grid.itemAtIndex(i).input_text === "" || !grid.itemAtIndex(i).isAccepted)
                    {
                        grid.isComplete = false
                        break
                    }
                }
                if(input_text.length <= 1)
                {
                    comp_popup.close()
                    _delega.isAccepted = false
                }
                else
                {
                    if (!comp_popup.visible)
                    {
                        comp_popup.x = _delega.x
                        comp_popup.y = _delega.y + _delega.height
                        comp_popup.width = _delega.width
                        comp_popup.index = index

                        comp_popup.open()
                    }

                    var _comp_vars = bsApp.completeBIP39dic(input_text)

                     _delega.isValid = true
                    comp_popup.not_valid_word = false

                    if (_comp_vars.length === 1)
                    {
                        comp_popup.comp_vars = _comp_vars
                        if (!_delega.isAccepted)
                        {
                            completer_accepted()
                        }
                    }
                    else
                    {
                        _delega.isAccepted = false
                        if (_comp_vars.length === 0)
                        {
                            _delega.isValid = false
                            comp_popup.not_valid_word = true
                            _comp_vars = ["Not a valid word"]
                            console.log("comp_popup.not_valid_word === true")
                        }

                        comp_popup.comp_vars = _comp_vars
                    }
                }
            }

            onActiveFocusChanged: {
                if(!_delega.activeFocus)
                    completer_accepted()
            }

            Keys.onDownPressed: comp_popup.current_increment()

            Keys.onUpPressed: comp_popup.current_decrement()

            function completer_accepted()
            {

                if (comp_popup.visible && comp_popup.index === index)
                {
                    if (_delega.isValid)
                    {
                        input_text = comp_popup.comp_vars[comp_popup.current_index]
                        _delega.isAccepted = true
                    }
                    comp_popup.close()
                    comp_popup.comp_vars = []
                    if(index < grid.count - 1)
                        grid.itemAtIndex(index+1).setActiveFocus()
                    else
                        nextItemInFocusChain().forceActiveFocus()
                }
            }

            Keys.onEnterPressed: completer_accepted()

            Keys.onReturnPressed: completer_accepted()

        }

        CustomCompleterPopup {
            id: comp_popup

            visible: false

            onCompChoosed: {
                grid.itemAtIndex(comp_popup.index).completer_accepted()
            }
        }
    }

    CustomButton {
        id: import_but
        text: qsTr("Import")
        Layout.leftMargin: 25
        Layout.bottomMargin: 40
        width: 530
        enabled: grid.isComplete

        Component.onCompleted: {
            import_but.preferred = true
        }
        function click_enter() {
            if (!import_but.enabled) return

            for (var i=0; i<grid.count; i++)
            {
                layout.phrase.push(grid.itemAtIndex(i).input_text)
            }
            layout.sig_import()
            clear()
        }
    }

    Keys.onEnterPressed: {
         import_but.click_enter()
    }

    Keys.onReturnPressed: {
         import_but.click_enter()
    }

    function init()
    {
        clear()
        grid.itemAtIndex(0).setActiveFocus()
    }

    function clear()
    {
        for (var i=0; i<grid.count; i++)
        {
            grid.itemAtIndex(i).input_text = ""
        }
    }
}
