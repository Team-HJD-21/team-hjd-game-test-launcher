import QtQuick
import QtQuick.Controls

Button {
    id: control
    property bool prominent: false
    property bool quiet: false
    implicitHeight: 42
    implicitWidth: Math.max(100, contentItem.implicitWidth + 36)
    padding: 12
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    font.family: "Malgun Gothic"
    font.pixelSize: 13
    font.weight: Font.DemiBold
    contentItem: Text {
        text: control.text
        font: control.font
        color: !control.enabled ? "#72777C" : control.prominent ? "#17191C" : "#EDEEEA"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        radius: 5
        color: !control.enabled ? "#282C2F" : control.prominent ? control.down ? "#C6A54C" : control.hovered ? "#F3D784" : "#E8C76A" : control.down ? "#353B3F" : control.hovered ? "#30363B" : control.quiet ? "transparent" : "#252B30"
        border.width: control.activeFocus ? 2 : control.quiet ? 0 : 1
        border.color: control.activeFocus ? "#FFFFFF" : control.prominent ? "transparent" : "#3A4147"
        Behavior on color { ColorAnimation { duration: launcher.reduceMotion ? 0 : 90 } }
    }
    HoverHandler { cursorShape: control.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor }
    Accessible.name: text
}
