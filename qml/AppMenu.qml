import QtQuick
import QtQuick.Controls

Menu {
    id: control
    width: 278
    padding: 6
    margins: 10
    overlap: 4
    popupType: Popup.Item
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    font.family: "Malgun Gothic"
    font.pixelSize: 12
    delegate: AppMenuItem {}
    contentItem: ListView {
        implicitHeight: contentHeight
        model: control.contentModel
        currentIndex: control.currentIndex
        spacing: 2
        clip: true
        interactive: contentHeight > height
        boundsBehavior: Flickable.StopAtBounds
    }
    background: Rectangle {
        color: "#202020"
        radius: 9
        border.color: "#393939"
        border.width: 1
    }
    enter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: launcher.reduceMotion ? 0 : 90 } }
    exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: launcher.reduceMotion ? 0 : 70 } }
}
