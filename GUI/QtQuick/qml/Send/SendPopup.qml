import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15

import "../BsStyles"
import "../StyledControls"

CustomPopup {
    id: root

    objectName: "send_popup"

    navig_bar_width: 30

    _stack_view.initialItem: simple_details
    _arrow_but_visibility: !simple_details.visible

    SimpleDetails {
        id: simple_details
        visible: false

        onSig_continue: (signature) => {
            console.log(signature)
        }
    }
}
