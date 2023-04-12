import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15

import "../BsStyles"
import "../StyledControls"

ColumnLayout  {
    id: layout

    CustomTitleLabel {
        id: title

        Layout.alignment: Qt.AlignCenter
        Layout.preferredHeight : title.height

        text: qsTr("About")
    }

    CustomListItem {
        id: about_terminal

        Layout.alignment: Qt.AlignCenter
        Layout.topMargin: BSSizes.applyScale(10)

        //aliases
        icon_source: "qrc:/images/about_terminal.svg"
        title_text: qsTr("terminal.blocksettle.com")

        onClicked: {
            Qt.openUrlExternally("https://terminal.blocksettle.com")
        }
    }

    CustomListItem {
        id: about_hello

        Layout.alignment: Qt.AlignCenter
        Layout.topMargin: BSSizes.applyScale(10)

        //aliases
        icon_source: "qrc:/images/about_hello.svg"
        title_text: qsTr("hello@blocksettle.com")

        onClicked: {
            Qt.openUrlExternally("mailto:hello@blocksettle.com")
        }
    }

    CustomListItem {
        id: about_twitter

        Layout.alignment: Qt.AlignCenter
        Layout.topMargin: BSSizes.applyScale(10)

        //aliases
        icon_source: "qrc:/images/about_twitter.svg"
        title_text: qsTr("twitter.com/blocksettle")

        onClicked: {
            Qt.openUrlExternally("https://twitter.com/blocksettle")
        }
    }

    CustomListItem {
        id: about_telegram

        Layout.alignment: Qt.AlignCenter
        Layout.topMargin: BSSizes.applyScale(10)

        //aliases
        icon_source: "qrc:/images/about_telegram.svg"
        title_text: qsTr("t.me/blocksettle")

        onClicked: {
            Qt.openUrlExternally("https://t.me/blocksettle")
        }
    }

    CustomListItem {
        id: add_github

        Layout.alignment: Qt.AlignCenter
        Layout.topMargin: BSSizes.applyScale(10)

        //aliases
        icon_source: "qrc:/images/about_github.svg"
        title_text: qsTr("github.com/blocksettle/terminal")

        onClicked: {
            Qt.openUrlExternally("https://github.com/blocksettle/terminal")
        }
    }

    Label {
        Layout.fillWidth: true
        Layout.fillHeight: true
    }

    CustomLabel {
        id: version

        Layout.bottomMargin: BSSizes.applyScale(20)
        Layout.alignment: Qt.AlignBottom | Qt.AlignHCenter

        text: qsTr("version 1.000.244.999")
    }
}