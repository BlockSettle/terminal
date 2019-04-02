import QtQuick 2.9
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.0

import "../BsStyles"
import "../BsControls"
import "../js/helper.js" as JsHelper

CustomDialogWindow {
    id: root
    property bool acceptable: false
    property bool rejectable: false
    property bool abortConfirmation: false
    property int abortBoxType

    // suggested to use these functions to close dialog popup with animation
    //
    function acceptAnimated(){
        closeTimer.acceptRet = true
        closeTimer.start()
        closeAnimation.start()
    }

    function rejectAnimated(){
        closeTimer.acceptRet = false
        closeTimer.start()
        closeAnimation.start()
    }

    function closeAnimated(result){
        if (result) acceptAnimated()
        else rejectAnimated()
    }


    property int animationDuration: 100

    default property alias cContentItem: customContentContainer.data
    property alias cHeaderItem: customHeaderContainer.data
    property alias cFooterItem: customFooterContainer.data

    signal enterPressed()

    ////////////////////////////
    /// Dialogs chain management

    // if isNextChainDialogSet then listen next dialog for dialogsChainFinished
    property bool isNextChainDialogSet: false

    // when some dialog call second one we should listen second dialog for finished signal
    function setNextChainDialog(dialog) {
        isNextChainDialogSet = true
        dialog.dialogsChainFinished.connect(function(){ dialogsChainFinished() })
    }

    // emitted if this dialog finished
    signal dialogFinished()

    // emitted if this is signle dialog and it finished or if dioalgs chain finished
    signal dialogsChainFinished()

    Component.onCompleted: {
        cContentItem.parent = customContentContainer
        cHeaderItem.parent = customHeaderContainer
        cFooterItem.parent = customFooterContainer
closeChainTimer.start()
        accepted.connect(function(){
            console.log("closeChainTimer started")

            closeChainTimer.start() })
        rejected.connect(function(){
            console.log("closeChainTimer started")
            closeChainTimer.start() })

    }

    Timer {
        id: closeChainTimer
        interval: 1250
        running: false
        repeat: false
        onTriggered: {
            console.log("closeChainTimer onTriggered")
            if (!isNextChainDialogSet) dialogsChainFinished()
            if (!isNextChainDialogSet) dialogsChainFinished()
        }
    }


    header: Item{}
    footer: Item{}

    onClosed: {
        root.destroy()
    }

    onOpened: PropertyAnimation {
        id: showAnimation
        target: root
        property: "opacity";
        duration: animationDuration;
        from: 0; to: 1
    }

    onAboutToHide: closeAnimation
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
        property bool acceptRet
        interval: animationDuration
        onTriggered: acceptRet? accept() : reject()
    }

    contentItem: FocusScope {
        id: focus
        anchors.fill: parent
        anchors.margins: 0
        focus: true

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

            RowLayout {
                id: customHeaderContainer
            }
            RowLayout {
                id: customContentContainer
            }
            RowLayout {
                id: customFooterContainer
            }
        }
    }

    Behavior on contentWidth  {
        NumberAnimation { duration: 20 }
    }
    Behavior on contentHeight  {
        NumberAnimation { duration: 20 }
    }
}
