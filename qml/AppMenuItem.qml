import QtQuick
import QtQuick.Controls

MenuItem {
    id: control
    property string shortcutText: ""
    property bool reserveIndicator: checkable
    property bool destructive: false
    implicitHeight: 32
    implicitWidth: 250
    leftPadding: reserveIndicator ? 30 : 12
    rightPadding: 12
    topPadding: 6
    bottomPadding: 6
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    font.family: "Malgun Gothic"
    font.pixelSize: 12
    contentItem: Item {
        implicitHeight: 20
        implicitWidth: title.implicitWidth + keyHint.implicitWidth + (keyHint.visible ? 24 : 0)
        Text {
            id: title
            anchors.left: parent.left
            anchors.right: keyHint.visible ? keyHint.left : parent.right
            anchors.rightMargin: keyHint.visible ? 20 : 0
            anchors.verticalCenter: parent.verticalCenter
            text: control.text
            font: control.font
            color: !control.enabled ? "#858585" : control.destructive ? "#E8B3A8" : "#ECECEC"
            elide: Text.ElideRight
        }
        Text {
            id: keyHint
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            text: control.shortcutText
            visible: text.length > 0
            color: control.enabled ? "#B4B4B4" : "#747474"
            font.family: "Segoe UI"
            font.pixelSize: 11
        }
    }
    indicator: Text {
        x: 11
        y: (control.height-height)/2
        width: 13
        text: "✓"
        visible: control.checkable && control.checked
        color: control.enabled ? "#E2E2E2" : "#858585"
        font.family: "Segoe UI"
        font.pixelSize: 13
    }
    arrow: Text {
        x: control.width-width-control.rightPadding
        y: (control.height-height)/2
        text: "›"
        visible: !!control.subMenu
        color: control.enabled ? "#B4B4B4" : "#747474"
        font.pixelSize: 19
    }
    background: Rectangle {
        radius: 5
        color: control.enabled && (control.highlighted || control.hovered || control.visualFocus) ? control.down ? "#3D3D3D" : "#333333" : "transparent"
        border.width: control.visualFocus ? 1 : 0
        border.color: "#8A8A8A"
    }
    Accessible.name: text + (shortcutText.length ? ", " + shortcutText : "")
}
