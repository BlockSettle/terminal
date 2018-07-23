import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3
import QtQuick.Dialogs 1.2
import com.blocksettle.OfflineProc 1.0

Item {
    id: view

    ScrollView {
        anchors.fill: parent
        Layout.fillWidth: true
        clip:   true

        ColumnLayout {
            width:  parent.parent.width
            id: column

            Behavior on height {
                NumberAnimation { duration: 500 }
            }

            GridLayout {
                id: grid1
                columns:    2
                Layout.fillWidth: true
                Layout.topMargin: 5
                Layout.leftMargin: 10
                Layout.rightMargin: 10

                CustomHeader {
                    Layout.columnSpan: 2
                    text:   qsTr("Controls:")
                    height: 25
                    Layout.fillWidth: true
                    Layout.preferredHeight: 25
                }

                RowLayout {
                    Layout.fillWidth: true

                    CustomLabel {
                        Layout.fillWidth: true
                        text:   qsTr("Online mode")
                    }
                }

                CustomSwitch {
                    Layout.alignment: Qt.AlignRight
                    text:   signerStatus.socketOk ? "" : qsTr("Failed to bind")
                    checked: !signerStatus.offline
                    onClicked: {
                        signerParams.offline = !checked
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40

                    CustomLabel {
                        Layout.fillWidth: true
                        text:   qsTr("Auto sign")
                    }
                }

                CustomSwitch {
                    Layout.alignment: Qt.AlignRight
                    visible:    !signerStatus.offline
                    checked:    signerStatus.autoSignActive
                    onClicked: {
                        if (checked) {
                            signerStatus.activateAutoSign()
                        }
                        else {
                            signerStatus.deactivateAutoSign()
                        }
                    }
                }

            }

            ColumnLayout{
                id: c1
                Layout.fillWidth: true


                GridLayout {
                    id: gridDashboard
                    columns:    2
                    Layout.leftMargin: 10
                    Layout.rightMargin: 10


                    CustomHeader {
                        Layout.fillWidth: true
                        Layout.columnSpan: 2
                        text:   qsTr("Details")
                        Layout.preferredHeight: 25
                    }


                    CustomLabel {
                        Layout.fillWidth: true
                        visible: !signerStatus.offline
                        text:   qsTr("Listen socket")
                        Layout.preferredHeight: 25

                    }
                    CustomLabelValue {
                        visible: !signerStatus.offline
                        text:   signerStatus.listenSocket
                        color:  signerStatus.socketOk ? "white" : "red"
                        Layout.alignment: Qt.AlignRight
                    }

                    CustomLabel {
                        Layout.fillWidth: true
                        visible: !signerStatus.offline
                        text:   qsTr("Network type")
                        Layout.preferredHeight: 25

                    }
                    CustomLabelValue {
                        visible: !signerStatus.offline
                        text:   signerParams.testNet ? qsTr("Testnet") : qsTr("Mainnet")
                        Layout.alignment: Qt.AlignRight
                    }

                    CustomLabel {
                        Layout.fillWidth: true
                        visible: !signerStatus.offline
                        text: qsTr("Connections")
                        Layout.preferredHeight: 25
                    }
                    CustomLabel {
                        visible: !signerStatus.offline && !signerStatus.connections
                        Layout.alignment: Qt.AlignRight
                        text:   qsTr("None")
                        padding: 5
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
                                text:   modelData
                                Layout.alignment: Qt.AlignRight
                            }
                        }
                    }

                    CustomLabel {
                        text:   qsTr("Transaction[s] signed")
                        Layout.fillWidth: true
                        Layout.preferredHeight: 25
                    }
                    CustomLabelValue {
                        text:   Number(signerStatus.txSignedCount)
                        opacity:  signerStatus.txSignedCount > 0 ? 1 : 0.5
                        Layout.alignment: Qt.AlignRight
                    }

                    CustomLabel {
                        text:   qsTr("Manual spend limit")
                        Layout.preferredHeight: 25
                    }
                    CustomLabelValue {
                        text:   qsTr("%1 of %2").arg(signerStatus.manualSignSpent.toFixed(8))
                        .arg(signerStatus.manualSignUnlimited ? qsTr("Unlimited") : signerStatus.manualSignLimit.toFixed(8))
                        Layout.alignment: Qt.AlignRight
                    }

                    CustomLabel {
                        visible: !signerStatus.offline
                        text:   qsTr("Auto-Sign spend limit")
                        Layout.preferredHeight: 25
                    }
                    CustomLabelValue {
                        visible: !signerStatus.offline
                        text:   qsTr("%1 of %2").arg(signerStatus.autoSignSpent.toFixed(8))
                        .arg(signerStatus.autoSignUnlimited ? qsTr("Unlimited") : signerStatus.autoSignLimit.toFixed(8))
                        Layout.alignment: Qt.AlignRight
                    }

                    CustomLabel {
                        visible: !signerStatus.offline
                        text:   qsTr("Auto-Sign time limit");
                        Layout.preferredHeight: 25
                    }
                    CustomLabelValue {
                        visible: !signerStatus.offline
                        text:   qsTr("%1 of %2")
                        .arg(signerStatus.autoSignTimeSpent ? signerStatus.autoSignTimeSpent : qsTr("None"))
                        .arg(signerStatus.autoSignTimeLimit ? signerStatus.autoSignTimeLimit : qsTr("Unlimited"))
                        Layout.alignment: Qt.AlignRight
                    }
                }
            }

            Rectangle {
                implicitHeight: view.height - grid1.height - c1.height
                                - btns.height - column.spacing * 4
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "#1c2835"
            }

            CustomButtonBar {
                Layout.fillWidth: true
                implicitHeight: childrenRect.height
                id: btns

                Flow {
                    id: buttonRow
                    spacing: 5
                    padding: 5
                    width: parent.width

                    CustomButton {
                        id: btnSignOfflineTx
                        text: qsTr("Sign Offline From File")
                        width: parent.width - 10
                        onClicked: {
                            if (!ldrOfflineFileDlg.item) {
                                ldrOfflineFileDlg.active = true
                            }
                            ldrOfflineFileDlg.item.open();
                        }
                    }
                }
            }
        }
    }

    Connections {
        target: offlineProc
        onSignSuccess: {
            ibSuccess.displayMessage(qsTr("Offline request successfully signed"))
        }
        onSignFailure: {
            ibFailure.displayMessage(qsTr("Failed to sign offline request - check log for details"))
        }
    }

    Loader {
        id:     ldrOfflineFileDlg
        active: false
        sourceComponent: FileDialog {
            id:             dlgOfflineFile
            visible:        false
            title:          qsTr("Select TX request file")
            nameFilters:    ["Offline TX requests (*.bin)", "All files (*)"]
            folder:         shortcuts.documents

            onAccepted: {
                var filePath = fileUrl.toString()
                filePath = filePath.replace(/(^file:\/{3})/, "")
                filePath = decodeURIComponent(filePath)

                var reqId = offlineProc.parseFile(filePath)
                offlineProc.processRequest(reqId)
            }
        }
    }
}
