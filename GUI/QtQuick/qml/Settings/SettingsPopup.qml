import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15

import "../BsStyles"
import "../StyledControls"

CustomPopup {
    id: root

    objectName: "settings_popup"

    _stack_view.initialItem: settings_menu
    _arrow_but_visibility: !settings_menu.visible

    SettingsMenu {
        id: settings_menu
        visible: false

        onSig_general: {
            _stack_view.push(settings_general)
            settings_general.init()
        }

        onSig_network: {
            _stack_view.push(settings_network)
            settings_network.init()
        }

        onSig_about: {

        }
    }

    SettingsGeneral {
        id: settings_general
        visible: false
    }

    SettingsNetwork {
        id: settings_network
        visible: false
    }

    function init() {
        settings_menu.init()
    }
}
