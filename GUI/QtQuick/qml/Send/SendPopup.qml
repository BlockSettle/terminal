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
    _arrow_but_visibility: !simple_details.visible && !advanced_details.visible

    SimpleDetails {
        id: simple_details
        visible: false

        onSig_continue: (signature) => {
            sign_trans.txSignRequest = signature
            _stack_view.push(sign_trans)
            sign_trans.init()
        }

        onSig_advanced: {
            _stack_view.replace(advanced_details)
            advanced_details.init()
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

    AdvancedDetails {
        id: advanced_details
        visible: false

        onSig_continue: (signature) => {
            sign_trans_advanced.txSignRequest = signature
            _stack_view.push(sign_trans_advanced)
            sign_trans_advanced.init()
        }

        onSig_simple: {
            _stack_view.replace(simple_details)
            simple_details.init()
        }

        onSig_select_inputs: {
            _stack_view.push(select_inputs)
            select_inputs.init()
        }
    }

    SelectInputs {
        id: select_inputs
        visible: false
    }

    SignTransactionAdvanced {
        id: sign_trans_advanced
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

    onSig_close_click: {
        if (simple_details.visible)
        {
            stack_create_wallet.pop()
        }
        else
        {
            root.close()
            stack_create_wallet.pop(null)
        }
    }
}
