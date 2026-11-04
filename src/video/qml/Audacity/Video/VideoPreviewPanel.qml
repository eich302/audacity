/*
* Audacity: A Digital Audio Editor
*/
import QtQuick

import Muse.Ui
import Muse.UiComponents
import Muse.Dock

import Audacity.Video

Item {
    id: root

    property alias currentTime: model.currentTime
    readonly property bool hasVideo: model.hasVideo

    //! Set by the dock panel hosting this, which is also what moves and resizes it
    property int panelLocation: Location.Right
    property bool panelLarge: false

    signal moveRequested(int location)
    signal sizeToggleRequested()

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

    Grid {
        id: controls

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.margins: 4
        height: model.hasVideo ? implicitHeight : 0

        visible: model.hasVideo
        spacing: 8

        //! One line when docked at the top or bottom, where height is scarce and width isn't
        columns: root.width >= frameControls.implicitWidth + layoutControls.implicitWidth + spacing + 8 ? 2 : 1
        horizontalItemAlignment: Grid.AlignHCenter

        Row {
            id: frameControls

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

        Row {
            id: layoutControls

            spacing: 4

            Repeater {
                model: [
                    { location: Location.Left, icon: IconCode.SMALL_ARROW_LEFT, title: qsTrc("video", "Move preview to the left") },
                    { location: Location.Right, icon: IconCode.SMALL_ARROW_RIGHT, title: qsTrc("video", "Move preview to the right") },
                    { location: Location.Top, icon: IconCode.SMALL_ARROW_UP, title: qsTrc("video", "Move preview to the top") },
                    { location: Location.Bottom, icon: IconCode.SMALL_ARROW_DOWN, title: qsTrc("video", "Move preview to the bottom") }
                ]

                FlatButton {
                    required property var modelData

                    icon: modelData.icon
                    toolTipTitle: modelData.title
                    accentButton: root.panelLocation === modelData.location
                    onClicked: root.moveRequested(modelData.location)
                }
            }

            FlatButton {
                icon: root.panelLarge ? IconCode.ZOOM_OUT : IconCode.ZOOM_IN
                toolTipTitle: root.panelLarge ? qsTrc("video", "Make preview smaller") : qsTrc("video", "Make preview larger")
                onClicked: root.sizeToggleRequested()
            }
        }
    }
}
