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

    property var armoryServersModel: ({})

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

        armoryServersModel: root.armoryServersModel

        onSig_add_custom: {
            _stack_view.push(add_armory_server)
            add_armory_server.init()
        }

        onSig_delete_custom: (server_index) => {
            _stack_view.push(delete_armory_server)
            delete_armory_server.init()
            delete_armory_server.server_index = server_index
        }
    }

    AddArmoryServer {
        id: add_armory_server
        visible: false

        armoryServersModel: root.armoryServersModel

        onSig_added: {
            _stack_view.pop()
        }
    }

    DeleteArmoryServer {
        id: delete_armory_server
        visible: false

        armoryServersModel: root.armoryServersModel

        onSig_delete: {
            _stack_view.pop()
        }

        onSig_back: {
            _stack_view.pop()
        }
    }

    function init() {
        settings_menu.init()
    }

    Component.onCompleted: {
        root.armoryServersModel = bsApp.getArmoryServers()
    }
}
