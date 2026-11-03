/*
* Audacity: A Digital Audio Editor
*/
import QtQuick

import Muse.Ui
import Muse.UiComponents

import Audacity.Video

Item {
    id: root

    property alias currentTime: model.currentTime
    readonly property bool hasVideo: model.hasVideo

    function setTime(secs) {
        model.setTime(secs)
    }

    VideoPreviewModel {
        id: model
    }

    Component.onCompleted: {
        model.load()
    }

    Rectangle {
        id: frameArea

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: controls.top
        anchors.margins: 4

        visible: model.hasVideo

        radius: 4
        color: "#1A1A1A"
        border.width: 1
        border.color: ui.theme.strokeColor
        clip: true

        Image {
            anchors.fill: parent
            anchors.margins: 2

            source: "image://video/" + model.frameId
            fillMode: Image.PreserveAspectFit
            cache: false
            asynchronous: false
        }
    }

    Row {
        id: controls

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.margins: 4
        height: model.hasVideo ? implicitHeight : 0

        visible: model.hasVideo
        spacing: 8

        FlatButton {
            anchors.verticalCenter: parent.verticalCenter
            icon: IconCode.ARROW_LEFT
            toolTipTitle: qsTrc("video", "Previous frame")
            onClicked: model.stepFrame(-1)
        }

        StyledTextLabel {
            id: timeLabel

            anchors.verticalCenter: parent.verticalCenter
            text: model.timeText
            horizontalAlignment: Text.AlignHCenter
        }

        FlatButton {
            anchors.verticalCenter: parent.verticalCenter
            icon: IconCode.ARROW_RIGHT
            toolTipTitle: qsTrc("video", "Next frame")
            onClicked: model.stepFrame(1)
        }
    }
}
