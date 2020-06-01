/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
import QtQuick 2.9
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.0
import QtQuick.Window 2.1
import com.blocksettle.QmlFactory 1.0
import QtQuick.Controls 1.4 as FirstControl

import "../BsStyles"
import "../BsControls"
import "../js/helper.js" as JsHelper

CustomDialogWindow {
    id: root

    property bool isPrepared: false
    property bool acceptable: false
    property bool rejectable: false
    property bool abortConfirmation: false
    property int  abortBoxType

    property int cContentHeight: customContentContainer.height
    property int cFooterHeight: customFooterContainer.height
    property int cHeaderHeight: customHeaderContainer.height

    property alias customContentContainer: customContentContainer
    property alias customFooterContainer: customFooterContainer
    property alias customHeaderContainer: customHeaderContainer
    property alias runSpinner: busyIndicator.running

//    onCContentHeightChanged: {
//        console.log("onCContentHeightChanged " + root + " " + cContentHeight)
//    }

//    onCFooterHeightChanged: {
//        console.log("onCFooterHeightChanged " + root + " " + cFooterHeight)
//    }

//    onCHeaderHeightChanged: {
//        console.log("onCHeaderHeightChanged " + root + " " + cHeaderHeight)
//    }

    ///////////////////
    // suggested to use these functions to close dialog popup with animation
    // dialog will be rejected on animatin finished
    // or after next dialog in chain will send dialogsChainFinished signal
    signal bsAccepted()
    signal bsRejected()

    function acceptAnimated(){
        bsAccepted()
        closeTimer.start()
        closeAnimation.start()
    }

    function rejectAnimated(){
        bsRejected()
        closeTimer.start()
        closeAnimation.start()
    }

    function closeAnimated(result){
        if (result) acceptAnimated()
        else rejectAnimated()
    }

    function hideMainWindow() {
        if (applyDialogClosing()) {
            rejectAnimated();
        }
    }

    // override this function where needed
    function applyDialogClosing() {
        return true;
    }

    property int animationDuration: 100

    default property alias cContentItem: customContentContainer.data
    property alias cHeaderItem: customHeaderContainer.data
    property alias cFooterItem: customFooterContainer.data

    signal enterPressed()

    // this signal used in light mode to inform mainwindow if size of dialog is changed
    // (for example if it's multipage dialog, or another popup doalog shown above current
    signal sizeChanged(int w, int h)

    onWidthChanged: {
        //console.log("CustomDialog.qml onWidthChanged " + root + " " + root.width + " " + root.height)

        if (root.width > Screen.desktopAvailableWidth) {
            //console.log("CustomDialog.qml Screen width fix")
            root.width = Screen.desktopAvailableWidth - 16
        }
        sizeChanged(root.width, root.height)
    }
    onHeightChanged: {
        //console.log("CustomDialog.qml onHeightChanged " + root + " " + root.width + " " + root.height)

        if (root.height > Screen.desktopAvailableHeight) {
            //console.log("CustomDialog.qml Screen height fix")
            let h = qmlFactory.titleBarHeight() + 16 // + extra window margins
            root.height = Screen.desktopAvailableHeight - h
        }
        sizeChanged(root.width, root.height)
    }

    ////////////////////////////
    /// Dialogs chain management

    // if isNextChainDialogSet then listen next dialog for dialogsChainFinished
    property bool isNextChainDialogSet: false
    property var  nextChainDialog: ({})

    // when some dialog call second one we should listen second dialog for finished signal
    function setNextChainDialog(dialog) {
        isNextChainDialogSet = true
        nextChainDialog = dialog
        nextChainDialogChangedOverloaded(dialog)
        dialog.dialogsChainFinished.connect(function(){
            dialogsChainFinished()
            reject()
        })
        dialog.nextChainDialogChangedOverloaded.connect(function(nextDialog){
            nextChainDialogChangedOverloaded(nextDialog)
        })
    }

    signal nextChainDialogChangedOverloaded(var nextDialog)

    // emitted if this is signle dialog and it finished or if dioalgs chain finished
    signal dialogsChainFinished()

    Component.onCompleted: {
        cContentItem.parent = customContentContainer
        cHeaderItem.parent = customHeaderContainer
        cFooterItem.parent = customFooterContainer
    }

    header: Item{}
    footer: Item{}

    onClosed: {
        if (!isNextChainDialogSet) {
            root.destroy()
        }
        else {
            dialogsChainFinished.connect(function(){ root.destroy() })
        }
    }

    onOpened: PropertyAnimation {
        id: showAnimation
        target: root
        property: "opacity";
        duration: animationDuration;
        from: 0; to: 1
    }

    //onAboutToHide: closeAnimation
    onBsAccepted: closeAnimation
    onBsRejected: closeAnimation


    PropertyAnimation {
        id: closeAnimation
        target: root
        property: "opacity";
        duration: animationDuration;
        from: 1; to: 0
    }

    Timer {
        // used to close dialog when close animation completed
        id: closeTimer
        interval: animationDuration
        onTriggered: {
            if (!isNextChainDialogSet) {
                dialogsChainFinished()
                reject()
            }
            else {
                reject()
            }
        }
    }

    contentItem: FocusScope {
        id: contentItemScope
        anchors.fill: parent
        anchors.margins: 0
        focus: true
//        Layout.alignment: Qt.AlignTop
//        Layout.fillHeight: true
//        Layout.margins: 0

        Keys.onPressed: {
            event.accepted = true
            if (event.modifiers === Qt.ControlModifier)
                switch (event.key) {
                case Qt.Key_A:
                    // detailedText.selectAll()
                    break
                case Qt.Key_C:
                    // detailedText.copy()
                    break
                case Qt.Key_Period:
                    if (Qt.platform.os === "osx") {
                        if (rejectable) rejectAnimated()
                        if (abortConfirmation) JsHelper.openAbortBox(root, abortBoxType)
                    }
                    break
            } else switch (event.key) {
                case Qt.Key_Escape:
                case Qt.Key_Back:
                    if (rejectable) rejectAnimated()
                    if (abortConfirmation) JsHelper.openAbortBox(root, abortBoxType)
                    break
                case Qt.Key_Enter:
                case Qt.Key_Return:
                    if (acceptable) acceptAnimated()
                    else enterPressed()
                    break
            }
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 0
            spacing: 0
            Layout.alignment: Qt.AlignTop
            Layout.margins: 0
            //Layout.fillHeight: true
            clip: true

            ColumnLayout {
                id: customHeaderContainer
                Layout.alignment: Qt.AlignTop
                Layout.margins: 0
                spacing: 0
                clip: true
            }
            ColumnLayout {
                id: customContentContainer
                //Layout.fillHeight: true
                Layout.alignment: Qt.AlignTop
                spacing: 0
                Layout.margins: 0
                clip: true
            }
            ColumnLayout {
                id: customFooterContainer
                Layout.alignment: Qt.AlignBottom
                spacing: 0
                Layout.margins: 0
                clip: true
            }
        }

        FirstControl.BusyIndicator {
            id: busyIndicator
            anchors.centerIn: parent
            running: false

            height: 50
            width: 50
        }
    }

    Behavior on contentWidth  {
        NumberAnimation { duration: 20 }
    }
    Behavior on contentHeight  {
        NumberAnimation { duration: 20 }
    }
}
