import QtQuick
import QtQuick.Controls

ToolTip {
    id: control
    x: parent ? (parent.width-width)/2 : 0
    y: parent ? parent.height+7 : 0
    margins: 10
    padding: 9
    delay: 650
    timeout: 4000
    contentItem: Text {
        text: control.text
        color: "#E5E5E5"
        font.family: "Malgun Gothic"
        font.pixelSize: 11
        wrapMode: Text.WordWrap
    }
    background: Rectangle { color: "#292929"; radius: 5; border.color: "#484848" }
}
