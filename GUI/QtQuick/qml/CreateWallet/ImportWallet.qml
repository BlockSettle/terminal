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

    height: BSSizes.applyScale(radbut_12.checked ? 555 : 670)
    width: BSSizes.applyScale(radbut_12.checked? 580: 760)

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
        Layout.topMargin: BSSizes.applyScale(24)

        isFullChoosed: true

        onSig_full_changed: (isFull) => {
            if (isFull === false)
            {
                type_switch.isFullChoosed = true
                layout.sig_only_watching()
            }
        }
    }

    RowLayout {
        id: row
        spacing: BSSizes.applyScale(12)

        Layout.alignment: Qt.AlignCenter
        Layout.topMargin: BSSizes.applyScale(32)
        Layout.preferredHeight: BSSizes.applyScale(19)

        Label {
            Layout.fillWidth: true
        }

        Label {
            id: radbut_text

            text: qsTr("Seed phrase type:")

            Layout.leftMargin: BSSizes.applyScale(25)

            width: BSSizes.applyScale(126)
            height: BSSizes.applyScale(19)

            color: "#E2E7FF"
            font.pixelSize: BSSizes.applyScale(16)
            font.family: "Roboto"
            font.weight: Font.Normal
        }

        CustomRadioButton {
            id: radbut_12

            text: "12 words"

            spacing: BSSizes.applyScale(6)
            font.pixelSize: BSSizes.applyScale(13)
            font.family: "Roboto"
            font.weight: Font.Normal

            checked: true
        }

        CustomRadioButton {
            id: radbut_24

            text: "24 words"

            spacing: BSSizes.applyScale(6)
            font.pixelSize: BSSizes.applyScale(13)
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

        Layout.topMargin: BSSizes.applyScale(16)
        Layout.fillWidth: true

        height: 1

        color: BSStyle.defaultGreyColor
    }

    GridView {
        id: grid

        Layout.fillHeight: true
        Layout.fillWidth: true
        Layout.leftMargin: BSSizes.applyScale(25)
        Layout.topMargin: BSSizes.applyScale(24)

        cellHeight : BSSizes.applyScale(56)
        cellWidth : BSSizes.applyScale(180)

        property bool isValid: true
        property bool isEmpty: true
        property bool hasEmptyWords: true

        model: radbut_12.checked ? layout.grid_model_12 : layout.grid_model_24
        delegate: CustomSeedTextInput {
            id: _delega

            property bool isAccepted: false

            width: BSSizes.applyScale(170)
            title_text: modelData
            onTextEdited : {
                show_fill_in_completer()
            }

            onActiveFocusChanged: {
                if(_delega.activeFocus)
                    show_fill_in_completer()
            }

            onEditingFinished: {
                completer_accepted()
                check_input()
                grid.validate()
            }

            Keys.onDownPressed: comp_popup.current_increment()

            Keys.onUpPressed: comp_popup.current_decrement()

            function show_fill_in_completer()
            {
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
                            change_focus()
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
                        }

                        comp_popup.comp_vars = _comp_vars
                    }
                }

                grid.validate()
                grid.check_empty_words()
            }

            function completer_accepted()
            {
                if (comp_popup.visible && comp_popup.index === index)
                {
                    if (_delega.isValid)
                    {
                        input_text = comp_popup.comp_vars[comp_popup.current_index]
                        _delega.isAccepted = true
                        _delega.isValid = true

                        grid.validate()
                    }
                    comp_popup.close()
                    comp_popup.comp_vars = []
                    comp_popup.not_valid_word = false
                }
            }

            function change_focus()
            {
                if(index < grid.count - 1)
                    grid.itemAtIndex(index+1).setActiveFocus()
                else
                    import_but.forceActiveFocus()
            }

            function check_input()
            {
                _delega.isValid = false

                var _comp_vars = bsApp.completeBIP39dic(input_text)
                for(var i=0; i<_comp_vars.length; i++)
                {
                    if (input_text === _comp_vars[i])
                    {
                        _delega.isValid = true
                        break
                    }
                }

                return _delega.isValid
            }

            Keys.onEnterPressed: {
                change_focus()
            }

            Keys.onReturnPressed: {
                change_focus()
            }
        }

        CustomCompleterPopup {
            id: comp_popup

            visible: false

            onCompChoosed: {
                grid.itemAtIndex(comp_popup.index).completer_accepted()
            }
        }

        function validate()
        {
            grid.isValid = true
            grid.isEmpty = true
            for (var i = 0; i < grid.count; i++) {
                if(!grid.itemAtIndex(i).isValid) {
                    grid.isValid = false
                    break
                }
                if (!grid.itemAtIndex(i).input_text.length > 0) {
                    grid.isEmpty = false
                    break
                }
            }
        }

        function check_empty_words()
        {
            grid.hasEmptyWords = false
            for (var i = 0; i < grid.count; i++)
            {
                var text = grid.itemAtIndex(i).input_text
                if(!text.length)
                {
                    grid.hasEmptyWords = true
                    break
                }
            }
        }
    }


    Label {
        id: error_description

        visible: !grid.isValid && !grid.isEmpty

        text:  qsTr("Invalid seed")

        Layout.bottomMargin: BSSizes.applyScale(24)
        Layout.alignment: Qt.AlignBottom | Qt.AlignHCenter
        Layout.preferredHeight : BSSizes.applyScale(16)

        height: BSSizes.applyScale(16)
        width: BSSizes.applyScale(136)

        color: "#EB6060"
        font.pixelSize: BSSizes.applyScale(14)
        font.family: "Roboto"
        font.weight: Font.Normal
    }

    CustomButton {
        id: import_but
        text: qsTr("Import")
        Layout.leftMargin: BSSizes.applyScale(25)
        Layout.bottomMargin: BSSizes.applyScale(40)

        width: BSSizes.applyScale(530)
        enabled: !grid.hasEmptyWords
        preferred: true
        Layout.alignment: Qt.AlignCenter

        function click_enter() {
            if (!import_but.enabled) return

            for (var i = 0; i < grid.count; i++)
            {
                grid.itemAtIndex(i).check_input()
            }

            if (!grid.isValid)
                return

            //Success!!!
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
        radbut_12.checked = true
        grid.itemAtIndex(0).setActiveFocus()
    }

    function clear()
    {
        for (var i=0; i<grid.count; i++)
        {
            grid.itemAtIndex(i).isValid = true
            grid.itemAtIndex(i).isAccepted = false
            grid.itemAtIndex(i).input_text = ""
        }
        grid.isValid = true
        grid.isEmpty = true
        grid.hasEmptyWords = true
    }
}
