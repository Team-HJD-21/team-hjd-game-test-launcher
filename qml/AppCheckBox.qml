import QtQuick
import QtQuick.Controls

CheckBox {
    id: control
    implicitHeight: 36
    padding: 4
    spacing: 10
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    font.family: "Malgun Gothic"
    font.pixelSize: 12
    indicator: Rectangle {
        implicitWidth: 18
        implicitHeight: 18
        x: control.leftPadding
        y: (control.height-height)/2
        radius: 4
        color: control.checked ? control.enabled ? "#E8C76A" : "#77715D" : control.down ? "#3A4146" : control.hovered ? "#30373C" : "#181D21"
        border.color: control.visualFocus ? "#F2DD9B" : control.checked ? "transparent" : "#68747D"
        border.width: control.visualFocus ? 2 : 1
        Text { anchors.centerIn: parent; text: "✓"; visible: control.checked; color: "#17191C"; font.family: "Segoe UI"; font.pixelSize: 13; font.bold: true }
    }
    contentItem: Text {
        text: control.text
        font: control.font
        color: control.enabled ? "#DCE2E3" : "#858D92"
        verticalAlignment: Text.AlignVCenter
        leftPadding: control.indicator.width+control.spacing
    }
    Accessible.name: text
}
