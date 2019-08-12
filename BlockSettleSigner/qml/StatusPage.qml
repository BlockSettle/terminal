import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3
import Qt.labs.platform 1.1
import com.blocksettle.WalletsProxy 1.0

import "StyledControls"
import "BsControls"
import "BsStyles"
import "js/helper.js" as JsHelper

Item {
    id: view

    ScrollView {
        anchors.fill: parent
        Layout.fillWidth: true
        clip: true

        ColumnLayout {
            width: parent.parent.width
            id: column

            CustomButtonBar {
                Layout.fillWidth: true
                implicitHeight: childrenRect.height
                id: btns

                Flow {
                    id: buttonRow
                    spacing: 5
                    padding: 5
                    width: parent.width

                    CustomButtonPrimary {
                        id: btnSignOfflineTx
                        text: qsTr("Sign Offline From File")
                        width: 200
                        onClicked: dlgOfflineFile.open()
                        FileDialog {
                            id: dlgOfflineFile
                            title: qsTr("Select TX request file")

                            nameFilters: ["Offline TX requests (*.bin)", "All files (*)"]
                            folder: StandardPaths.writableLocation(StandardPaths.DocumentsLocation)
                            fileMode: FileDialog.OpenFile

                            onAccepted: {
                                let filePath = qmlAppObj.getUrlPath(file)
                                var signCallback = function(success, msg) {
                                    if (success) {
                                        JsHelper.messageBox(BSMessageBox.Type.Success
                                            , qsTr("Sign Offline TX"), qsTr("Offline TX successfully signed"), msg)

                                    } else {
                                        JsHelper.messageBox(BSMessageBox.Type.Critical
                                            , qsTr("Sign Offline TX"), qsTr("Signing Offline TX failed with error:"), msg)
                                    }
                                }

                                walletsProxy.signOfflineTx(filePath, signCallback)
                            }
                        }
                    }
                }
            }

            Behavior on height {
                NumberAnimation { duration: 500 }
            }

            GridLayout {
                id: grid1
                columns: 2
                Layout.fillWidth: true
                Layout.topMargin: 5
                Layout.leftMargin: 10
                Layout.rightMargin: 10

                CustomHeader {
                    Layout.columnSpan: 2
                    text: qsTr("Controls")
                    height: 25
                    Layout.fillWidth: true
                    Layout.preferredHeight: 25
                }

                RowLayout {
                    Layout.fillWidth: true

                    CustomLabel {
                        Layout.fillWidth: true
                        text: qsTr("Online mode")
                    }
                }

                CustomSwitch {
                    Layout.alignment: Qt.AlignRight
                    text: signerStatus.socketOk ? "" : qsTr("Failed to bind")
                    checked: !signerStatus.offline
                    onClicked: {
                        signerSettings.offline = !checked
                    }
                }

//                RowLayout {
//                    Layout.fillWidth: true
//                    Layout.preferredHeight: 25

//                    CustomLabel {
//                        Layout.fillWidth: true
//                        text: qsTr("Auto-Sign")
//                    }
//                }

//                CustomSwitch {
//                    Layout.alignment: Qt.AlignRight
//                    visible: !signerStatus.offline
//                    checked: signerStatus.autoSignActive
//                    onClicked: {
//                        if (checked) {
//                            signerStatus.activateAutoSign()
//                        }
//                        else {
//                            signerStatus.deactivateAutoSign()
//                        }
//                    }
//                }

            }

            ColumnLayout{
                id: c1
                Layout.fillWidth: true


                GridLayout {
                    id: gridDashboard
                    columns: 2
                    Layout.leftMargin: 10
                    Layout.rightMargin: 10


                    CustomHeader {
                        Layout.fillWidth: true
                        Layout.columnSpan: 2
                        text: qsTr("Details")
                        Layout.preferredHeight: 25
                    }


                    CustomLabel {
                        Layout.fillWidth: true
                        text: qsTr("Listen socket")
                        Layout.preferredHeight: 25
                    }
                    CustomLabelValue {
                        text: (signerStatus.offline ? qsTr("Closed") : signerStatus.listenSocket)
                        color: (signerStatus.offline ? "white" : (signerStatus.socketOk ? "white" : "red"))
                        Layout.alignment: Qt.AlignRight
                        wrapMode: Text.NoWrap
                    }

                    CustomLabel {
                        Layout.fillWidth: true
                        text: qsTr("Network type")
                        Layout.preferredHeight: 25

                    }
                    CustomLabelValue {
                        text: signerSettings.testNet ? qsTr("Testnet") : qsTr("Mainnet")
                        Layout.alignment: Qt.AlignRight
                        wrapMode: Text.NoWrap
                    }

                    CustomLabel {
                        Layout.fillWidth: true
                        text: qsTr("Connection[s]")
                        Layout.preferredHeight: 25
                    }
                    CustomLabel {
                        visible: signerStatus.offline || !signerStatus.connections
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("None")
                        padding: 5
                        wrapMode: Text.NoWrap
                    }
                    ColumnLayout {
                        spacing: 0
                        visible: !signerStatus.offline && signerStatus.connections
                        Layout.leftMargin: 0
                        Layout.rightMargin: 0
                        Layout.alignment: Qt.AlignRight
                        Repeater {
                            model: signerStatus.connectedClients
                            CustomLabelValue {
                                text: modelData
                                Layout.alignment: Qt.AlignRight
                                wrapMode: Text.NoWrap
                            }
                        }
                    }

                    CustomLabel {
                        text: qsTr("Transaction[s] signed")
                        Layout.fillWidth: true
                        Layout.preferredHeight: 25
                    }
                    CustomLabelValue {
                        text: Number(signerStatus.txSignedCount)
                        opacity: signerStatus.txSignedCount > 0 ? 1 : 0.5
                        Layout.alignment: Qt.AlignRight
                        wrapMode: Text.NoWrap
                    }

                    CustomLabel {
                        text: qsTr("Manual spend limit")
                        Layout.preferredHeight: 25
                    }
                    CustomLabelValue {
                        text: qsTr("%1 of %2").arg(signerStatus.manualSignSpent.toFixed(8))
                        .arg(signerStatus.manualSignUnlimited ? qsTr("Unlimited") : signerStatus.manualSignLimit.toFixed(8))
                        Layout.alignment: Qt.AlignRight
                        wrapMode: Text.NoWrap
                    }

                    CustomLabel {
                        text: qsTr("Auto-Sign spend limit")
                        Layout.preferredHeight: 25
                    }
                    CustomLabelValue {
                        text: (signerStatus.offline ? qsTr("0.00000000 of Unlimited") :
                            qsTr("%1 of %2").arg(signerStatus.autoSignSpent.toFixed(8))
                                .arg(signerStatus.autoSignUnlimited ? qsTr("Unlimited") :
                                    signerStatus.autoSignLimit.toFixed(8)))
                        Layout.alignment: Qt.AlignRight
                        wrapMode: Text.NoWrap
                    }

                    CustomLabel {
                        text: qsTr("Auto-Sign time limit");
                        Layout.preferredHeight: 25
                    }
                    CustomLabelValue {
                        text: (signerStatus.offline ? qsTr("None of Unlimited") : qsTr("%1 of %2")
                        .arg(signerStatus.autoSignTimeSpent ? signerStatus.autoSignTimeSpent : qsTr("None"))
                        .arg(signerStatus.autoSignTimeLimit ? signerStatus.autoSignTimeLimit : qsTr("Unlimited")))
                        Layout.alignment: Qt.AlignRight
                        wrapMode: Text.NoWrap
                    }
                }
            }

            Rectangle {
                implicitHeight: view.height - grid1.height - c1.height
                                - btns.height - column.spacing * 4
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: BSStyle.backgroundColor
            }
        }
    }
}
