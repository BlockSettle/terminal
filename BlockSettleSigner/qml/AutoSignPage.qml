import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Controls.Styles 1.4
import QtQuick.Dialogs 1.2
import QtQuick.Layouts 1.3
import com.blocksettle.WalletsProxy 1.0

Item {
    ScrollView {
        anchors.fill: parent
        clip:   true

        ColumnLayout {
            width:  parent.parent.width

            GridLayout {
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
                    Layout.preferredHeight: 40

                    CustomLabel {
                        Layout.fillWidth: true
                        text:   qsTr("Wallet")
                    }
                }

                Loader {
                    active: walletsProxy.loaded
                    sourceComponent: CustomComboBox {
                        width: 200
                        enabled:    !signerStatus.autoSignActive
                        model: walletsProxy.walletNames
                        currentIndex: walletsProxy.indexOfWalletId(signerParams.autoSignWallet)
                        onActivated: {
                            signerParams.autoSignWallet = walletsProxy.walletIdForIndex(currentIndex)
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40

                    CustomLabel {
                        Layout.fillWidth: true
                        text:   qsTr("Auto Sign")
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

            SettingsGrid {
                id: gridLimits

                CustomHeader {
                    Layout.fillWidth: true
                    Layout.columnSpan: 2
                    text:   qsTr("Details")
                    Layout.preferredHeight: 25
                }

                CustomLabel {
                    text:   qsTr("XBT spend limit:")
                }
                CustomTextInput {
                    Layout.fillWidth: true
                    text:   signerParams.autoSignUnlimited ? qsTr("Unlimited") : signerParams.limitAutoSignXbt
                    selectByMouse: true
                    id: limitAutoSignXbt
                    onEditingFinished: {
                        if (text !== qsTr("Unlimited")) {
                            signerParams.limitAutoSignXbt = text
                        }
                    }
                }

                CustomLabel {
                    text:   qsTr("Time limit:")
                }
                CustomTextInput {
                    Layout.fillWidth: true
                    placeholderText: "e.g. 1h or 15min or 600s or combined"
                    selectByMouse: true
                    text:   signerParams.limitAutoSignTime ? signerParams.limitAutoSignTime : qsTr("Unlimited")
                    id: limitAutoSignTime
                    onEditingFinished: {
                        signerParams.limitAutoSignTime = text
                    }
                }
            }
        }
    }

    function storeSettings() {
        if (signerParams.limitAutoSignXbt != limitAutoSignXbt.text) {
            if (limitAutoSignXbt.text !== qsTr("Unlimited")) {
                signerParams.limitAutoSignXbt = limitAutoSignXbt.text
            }
        }
        if (signerParams.limitAutoSignTime !== limitAutoSignTime.text) {
            signerParams.limitAutoSignTime = limitAutoSignTime.text
        }
    }
}
