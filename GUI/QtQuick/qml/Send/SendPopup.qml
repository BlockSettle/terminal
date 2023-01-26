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
            sign_trans.txSignRequest = signature
            _stack_view.push(sign_trans)
            sign_trans.init()
        }
    }

    SignTransaction {
        id: sign_trans
        visible: false

        onSig_broadcast:  {
            root.close()
            _stack_view.pop(null)
        }

        onSig_time_finished:  {
            root.close()
            _stack_view.pop(null)
        }
    }

    function init() {
        simple_details.init()
    }
}
