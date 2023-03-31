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

    property var tx: null
    property bool isRBF: false
    property bool isCPFP: false

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

        tx: root.tx
        isRBF: root.isRBF
        isCPFP: root.isCPFP

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

        isRBF: root.isRBF
        isCPFP: root.isCPFP

        onSig_broadcast:  {
            root.close()
            _stack_view.pop(null)
        }

        onSig_time_finished:  {
            root.close()
            _stack_view.pop(null)
        }
    }

    CustomSuccessDialog {
        id: successDialog
        visible: false
    }

    CustomFailDialog {
        id: failDialog
        visible: false
    }

    Connections {
        target: bsApp
        onTransactionExported: (text) => {
            successDialog.details_text = qsTr("Transaction successfully exported to %1").arg(text)
            successDialog.open()
        }
        onTransactionExportFailed: (text) => {
            failDialog.header = qsTr("Export transaction failed")
            failDialog.fail = text
            failDialog.open()
        }
    }

    function init() {
        _stack_view.replace(bsApp.settingAdvancedTX ? advanced_details : simple_details)

        if (_stack_view.currentItem === simple_details)
        {
            simple_details.init()
        }
        else if (_stack_view.currentItem === advanced_details)
        {
            advanced_details.init()
        }
        root.tx = null
        root.isRBF = false
        root.isCPFP = false
    }

    function open(txId: string, isRBF: bool, isCPFP: bool)
    {
        _stack_view.replace(advanced_details)
        root.tx = bsApp.getTXDetails(txId)
        root.isRBF = isRBF
        root.isCPFP = isCPFP
        advanced_details.init()
    }

    function close_click()
    {
        if (select_inputs.visible)
        {
            _stack_view.pop()
        }
        else
        {
            root.close()
            _stack_view.pop(null)
        }
    }
}
