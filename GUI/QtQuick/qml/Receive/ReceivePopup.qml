import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15

import "../BsStyles"
import "../StyledControls"

CustomPopup {
    id: root

    objectName: "receive_popup"

    _stack_view.initialItem: receive_qr_code
    _arrow_but_visibility: !receive_qr_code.visible

    ReceiveQrCode {
        id: receive_qr_code
        visible: false
    }
}
