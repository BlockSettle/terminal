import QtQuick 2.9
import QtQuick.Controls 2.3
import QtQuick.Controls.Styles 1.4
import "../BsStyles"

// styled dialog popup
Dialog {
    x: (width > parent.width) ? 0 : (parent.width - width) / 2
    y: (height > parent.height) ? 0 : (parent.height - height) / 2

    focus: true
    modal: true
    closePolicy: Popup.NoAutoClose
    padding: 0
    background: Rectangle {
        color:  BSStyle.backgroundColor
        border.color: BSStyle.dialogHeaderColor
    }
}
