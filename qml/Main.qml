import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

ApplicationWindow {
    id: window
    width: 1160; height: 790
    minimumWidth: 1000; minimumHeight: 700
    visible: true
    flags: Qt.Window | Qt.FramelessWindowHint
    title: "TeamHJD Game Test Launcher"
    color: "#171A1D"
    palette.window: "#202428"
    palette.windowText: "#E6E6E6"
    palette.base: "#171B1E"
    palette.alternateBase: "#22282C"
    palette.text: "#E6E6E6"
    palette.button: "#292E32"
    palette.buttonText: "#E6E6E6"
    palette.light: "#363B3F"
    palette.midlight: "#303539"
    palette.mid: "#4C5358"
    palette.dark: "#15191C"
    palette.highlight: "#3A4247"
    palette.highlightedText: "#FFFFFF"
    palette.toolTipBase: "#292929"
    palette.toolTipText: "#E5E5E5"
    palette.disabled.windowText: "#858585"
    palette.disabled.text: "#858585"
    palette.disabled.buttonText: "#858585"
    font.family: "Malgun Gothic"
    font.pixelSize: 13
    property var v: launcher.view
    readonly property bool isHoseGame: v.name && v.name.toLowerCase() === "hose for hire"
    readonly property bool gameUpdateReady: !!v.installed && !!v.updateAvailable && !!v.online && !v.busy && !v.running
    property bool entering: true
    property bool nativeMaxHover: false
    property bool sidebarVisible: true
    property int unreadCount: 0
    property int nextNotice: 0
    property string reviewMode: ""
    property bool advancedSettings: false
    onReviewModeChanged: {
        if(reviewMode==="login") loginNotice.open()
        else if(reviewMode==="settings") settings.open()
        else if(reviewMode==="help") helpDialog.open()
        else if(reviewMode==="notifications") {window.notify("새 버전을 확인했어요", "게임은 업데이트 버튼을 눌러 적용하세요.", "success");notificationPanel.open();window.unreadCount=0}
        else if(reviewMode==="toast") window.notify("플레이할 준비가 되었어요", "설치 완료 알림의 표시와 읽기 흐름을 확인하고 있어요.", "success")
        else if(reviewMode==="versions") gameInfo.open()
        else if(reviewMode==="uninstall") uninstallDialog.open()
        else if(reviewMode==="menus") {gameMenu.open();gameMenu.currentIndex=0}
        else if(reviewMode==="menubar") {viewMenu.open();viewMenu.currentIndex=0}
        else if(reviewMode==="settings-bottom") {settings.open();reviewScrollTimer.start()}
    }
    Timer { id: reviewScrollTimer; interval: 160; onTriggered: formScroll.contentItem.contentY=Math.max(0,formScroll.contentItem.contentHeight-formScroll.contentItem.height) }
    ListModel { id: notificationHistory }
    ListModel { id: toastModel }
    function notify(title, message, tone) {
        var entry = {noticeTitle: title, noticeBody: message, noticeTone: tone, uid: ++nextNotice, time: Qt.formatTime(new Date(), "hh:mm")}
        notificationHistory.insert(0, entry)
        if(notificationHistory.count>30) notificationHistory.remove(30)
        if(!notificationPanel.opened) unreadCount++
        toastModel.append(entry)
        if(toastModel.count>3) toastModel.remove(0)
    }
    function removeToast(uid) { for(var i=0;i<toastModel.count;i++) if(toastModel.get(i).uid===uid) { toastModel.remove(i); return } }
    onClosing: function(close) { if (!launcher.canClose()) { close.accepted = false; closeNotice.open() } }
    Component.onCompleted: entering = false
    function connectInvite(file) { var result=launcher.importInvite(file);if(result.applied){settings.close();return}if(result.server){settings.open();serverField.text=result.server;keyField.text=result.key} }
    Connections { target: launcher; function onRequestSettings() { settings.open() } function onRequestInvite() { inviteDialog.open() } function onRequestLogin() { loginNotice.open() } function onInviteAccepted() {settings.close();passwordField.text=""} function onToast(title, message, tone) { window.notify(title, message, tone) } }
    DropArea {
        id: inviteDrop;anchors.fill: parent
        onDropped: function(drop) {if(drop.hasUrls&&drop.urls.length===1){var path=drop.urls[0].toString().toLowerCase();if(path.endsWith(".hjdinvite")||path.endsWith(".json")){window.connectInvite(drop.urls[0]);drop.acceptProposedAction()}}}
    }
    Rectangle { z:100;anchors.fill: parent;anchors.margins: 5;visible: inviteDrop.containsDrag;color: "#D01B2429";border.color: "#E8C76A";border.width: 2;radius: 7;Label {anchors.centerIn: parent;text: "초대 파일을 여기에 놓아 주세요";color: "#F0E4BD";font.pixelSize: 22;font.bold: true} }
    Shortcut { sequence: "Ctrl+,"; onActivated: settings.open() }
    Shortcut { sequence: "Ctrl+B"; onActivated: window.sidebarVisible=!window.sidebarVisible }
    Shortcut { sequence: "F1"; onActivated: helpDialog.open() }
    Shortcut { sequence: "Alt+F4"; onActivated: window.close() }

    component Field: TextField {
        implicitHeight: 44
        color: "#F0F0EA"
        placeholderTextColor: "#737D86"
        selectionColor: "#6C633F"
        selectedTextColor: "#FFFFFF"
        padding: 12
        font.pixelSize: 13
        background: Rectangle { radius: 4; color: "#161A1E"; border.color: parent.activeFocus ? "#E8C76A" : "#3D454D"; border.width: parent.activeFocus ? 2 : 1 }
        Accessible.name: placeholderText
    }
    component Caption: Label { color: "#9FA8AE"; font.pixelSize: 12; wrapMode: Text.WordWrap }
    component HelpTab: TabButton {
        id: tab
        implicitHeight: 36
        contentItem: Text { text: tab.text; color: tab.checked ? "#EFEFE8" : "#A5B1B9"; font.pixelSize: 13; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
        background: Rectangle { color: tab.checked ? "#303B41" : tab.hovered ? "#2D353B" : "#252D32"; border.width: tab.visualFocus ? 1 : 0; border.color: "#CFC3A1"; Rectangle { width: parent.width; height: 2; anchors.bottom: parent.bottom; color: tab.checked ? "#E8C76A" : "transparent" } }
    }

    Rectangle {
        id: titlebar
        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
        height: 40; color: "#1D2124"
        MouseArea { anchors.fill: parent; acceptedButtons: Qt.LeftButton; onPressed: window.startSystemMove(); onDoubleClicked: window.visibility===Window.Maximized ? window.showNormal() : window.showMaximized() }
        RowLayout {
            anchors.left: parent.left; anchors.leftMargin: 12; anchors.verticalCenter: parent.verticalCenter; spacing: 3
            ActionButton { text: "\uE700"; font.family: "Segoe Fluent Icons"; padding: 4; quiet: true; implicitWidth: 28; implicitHeight: 28; font.pixelSize: 14; Accessible.name: "사이드바 전환"; onClicked: window.sidebarVisible=!window.sidebarVisible }
            Image { source: "qrc:/assets/brand/TeamHJD-Simple-Logo.png"; Layout.preferredWidth: 88; Layout.preferredHeight: 24; fillMode: Image.PreserveAspectFit; mipmap: true; Layout.leftMargin: 3; Layout.rightMargin: 12 }
            ActionButton { id: fileMenuButton; text: "파일"; padding: 4; quiet: true; implicitWidth: 44; implicitHeight: 28; font.pixelSize: 11; onClicked: fileMenu.open() }
            ActionButton { id: viewMenuButton; text: "보기"; padding: 4; quiet: true; implicitWidth: 44; implicitHeight: 28; font.pixelSize: 11; onClicked: viewMenu.open() }
            ActionButton { id: helpMenuButton; text: "도움말"; padding: 4; quiet: true; implicitWidth: 58; implicitHeight: 28; font.pixelSize: 11; onClicked: helpMenu.open() }
        }
        Label { anchors.centerIn: parent; text: "TeamHJD Game Test Launcher"; color: "#8E989F"; font.family: "Segoe UI"; font.pixelSize: 11; visible: window.width>=1080 }
        Row {
            anchors.right: parent.right; height: parent.height
            ToolButton {
                id: notificationButton; width: 42; height: 40
                contentItem: Item { Image { anchors.centerIn: parent; width: 19; height: 19; source: "qrc:/assets/icons/bell.svg" } }
                Accessible.name: "알림"
                AppToolTip { parent: notificationButton; text: "알림"; visible: notificationButton.hovered }
                onClicked: { notificationPanel.open(); window.unreadCount=0 }
                background: Rectangle { color: notificationButton.hovered ? "#323A40" : "transparent"; border.width: notificationButton.visualFocus ? 1 : 0; border.color: "#BDAE83" }
                Rectangle { visible: window.unreadCount>0; width: 6; height: 6; radius: 3; color: "#E8C76A"; anchors.right: parent.right; anchors.rightMargin: 10; anchors.top: parent.top; anchors.topMargin: 8 }
            }
            Repeater {
                model: ["minimize", "maximize", "close"]
                delegate: ToolButton {
                    id: windowButton; required property string modelData
                    width: 46; height: 40
                    Accessible.name: modelData==="minimize" ? "최소화" : modelData==="maximize" ? "최대화 또는 복원" : "닫기"
                    onClicked: { if(modelData==="minimize") window.showMinimized(); else if(modelData==="maximize") window.visibility===Window.Maximized ? window.showNormal() : window.showMaximized(); else window.close() }
                    background: Rectangle { color: windowButton.hovered || (windowButton.modelData==="maximize" && window.nativeMaxHover) ? windowButton.modelData==="close" ? "#BA3C37" : "#353D43" : "transparent" }
                    contentItem: Item {
                        Rectangle { visible: windowButton.modelData==="minimize"; width: 10; height: 1; color: "#C8CFD3"; anchors.centerIn: parent }
                        Rectangle { visible: windowButton.modelData==="maximize"; width: 9; height: 9; color: "transparent"; border.color: "#C8CFD3"; anchors.centerIn: parent }
                        Rectangle { visible: windowButton.modelData==="close"; width: 12; height: 1; color: "#E1E5E7"; rotation: 45; anchors.centerIn: parent }
                        Rectangle { visible: windowButton.modelData==="close"; width: 12; height: 1; color: "#E1E5E7"; rotation: -45; anchors.centerIn: parent }
                    }
                }
            }
        }
        Rectangle { height: 1; color: "#2B3237"; anchors.bottom: parent.bottom; width: parent.width }
    }
    AppMenu {
        id: fileMenu; parent: fileMenuButton; y: fileMenuButton.height+6
        AppMenuItem { text: v.needsLogin ? "이메일로 연결…" : "이메일 다시 인증…"; visible: v.cloudflare; enabled: !v.busy; onTriggered: loginNotice.open() }
        AppMenuItem { text: "설정…"; shortcutText: "Ctrl+,"; onTriggered: settings.open() }
        AppMenuItem { text: "게임 설치 폴더 열기"; onTriggered: launcher.openFolder("root") }
        AppMenuSeparator {}
        AppMenuItem { text: "종료"; shortcutText: "Alt+F4"; onTriggered: window.close() }
    }
    AppMenu {
        id: viewMenu; parent: viewMenuButton; y: viewMenuButton.height+6; width: 290
        AppMenuItem { text: "사이드바 표시"; shortcutText: "Ctrl+B"; reserveIndicator: true; checkable: true; checked: window.sidebarVisible; onTriggered: window.sidebarVisible=checked }
        AppMenuItem { text: "알림 보기"; reserveIndicator: true; onTriggered: {notificationPanel.open();window.unreadCount=0} }
        AppMenuItem { text: "화면 설정…"; reserveIndicator: true; onTriggered: settings.open() }
    }
    AppMenu {
        id: helpMenu; parent: helpMenuButton; y: helpMenuButton.height+6
        AppMenuItem { text: "사용 안내"; shortcutText: "F1"; onTriggered: helpDialog.open() }
        AppMenuItem { text: "문제 해결"; onTriggered: { helpTabs.currentIndex=1; helpDialog.open() } }
        AppMenuSeparator {}
        AppMenuItem { text: "런처 정보"; onTriggered: {helpTabs.currentIndex=2;helpDialog.open()} }
    }

    Rectangle {
        id: sidebar
        width: window.sidebarVisible ? 222 : 0; anchors.top: titlebar.bottom; anchors.bottom: parent.bottom; clip: true
        Behavior on width { NumberAnimation { duration: launcher.reduceMotion ? 0 : 140 } }
        color: "#111416"
        Rectangle { width: 1; anchors.right: parent.right; height: parent.height; color: "#272D31" }
        ColumnLayout {
            anchors.fill: parent; anchors.margins: 22
            spacing: 0
            Image { source: "qrc:/assets/brand/TeamHJD-Simple-Logo.png"; Layout.preferredWidth: 155; Layout.preferredHeight: 38; fillMode: Image.PreserveAspectFit; horizontalAlignment: Image.AlignLeft; mipmap: true; Accessible.name: "Team HJD" }
            Caption { text: "GAME TEST LAUNCHER"; font.pixelSize: 9; font.letterSpacing: 1.65; color: "#8A939A"; Layout.topMargin: 8 }
            Label { text: "라이브러리"; color: "#77828A"; font.pixelSize: 11; font.bold: true; Layout.topMargin: 44; Layout.bottomMargin: 14 }
            Repeater {
                model: launcher.library
                delegate: ItemDelegate {
                    id: libraryItem
                    required property var modelData
                    Layout.fillWidth: true; Layout.preferredHeight: 64
                    padding: 12; hoverEnabled: true
                    onClicked: launcher.selectGame(modelData.id)
                    background: Rectangle { radius: 5; color: libraryItem.modelData.selected ? "#282E32" : libraryItem.hovered ? "#20262B" : "transparent"; border.width: libraryItem.activeFocus ? 2 : 0; border.color: "#E8C76A"; Rectangle { visible: libraryItem.modelData.selected; width: 3; height: 24; color: "#E8C76A"; anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter; radius: 1 } }
                    contentItem: RowLayout {
                        spacing: 9
                        Image { visible: libraryItem.modelData.id==="hose-for-hire" || libraryItem.modelData.name.toLowerCase()==="hose for hire"; source: "qrc:/assets/games/hose-for-hire-logo.png"; Layout.preferredWidth: 37; Layout.preferredHeight: 32; fillMode: Image.PreserveAspectFit; mipmap: true }
                        ColumnLayout { spacing: 4; Layout.fillWidth: true; Label { Layout.fillWidth: true; text: libraryItem.modelData.name; color: "#ECEDE8"; font.pixelSize: 12; font.bold: true; elide: Text.ElideRight } Label { text: "TEAMHJD PLAYTEST"; color: "#8C989F"; font.pixelSize: 8; font.letterSpacing: .3 } }
                    }
                    Accessible.name: modelData.name
                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                }
            }
            Item { Layout.fillHeight: true }
            Rectangle { Layout.fillWidth: true; height: 1; color: "#2A3035"; Layout.bottomMargin: 15 }
            ActionButton { text: "설정"; quiet: true; Layout.fillWidth: true; onClicked: settings.open() }
            ActionButton { text: "도움말 및 정보"; quiet: true; Layout.fillWidth: true; onClicked: helpDialog.open() }
            Caption { text: "v" + launcher.version + "  ·  Windows"; color: "#657078"; font.pixelSize: 10; Layout.topMargin: 14; Layout.leftMargin: 12 }
        }
    }

    ColumnLayout {
        anchors.left: sidebar.right; anchors.right: parent.right; anchors.top: titlebar.bottom; anchors.bottom: parent.bottom
        anchors.leftMargin: 36; anchors.rightMargin: 36; anchors.topMargin: 27; anchors.bottomMargin: 20
        spacing: 0
        opacity: window.entering ? 0 : 1
        Behavior on opacity { NumberAnimation { duration: launcher.reduceMotion ? 0 : 180 } }
        RowLayout {
            Layout.fillWidth: true; Layout.bottomMargin: 23
            Label { text: "플레이테스트"; font.pixelSize: 16; font.bold: true; color: "#E8EBE8" }
            Item { Layout.fillWidth: true }
            Rectangle { width: 6; height: 6; radius: 3; color: v.online ? "#8CBD9E" : "#7B8389" }
            Caption { text: v.checking ? "자동 확인 중" : v.online ? "새 버전 자동 확인" : v.hasServer ? "자동 재연결 중" : "서버 연결 전"; font.pixelSize: 11 }
        }
        Rectangle {
            id: hero
            Layout.fillWidth: true; Layout.preferredHeight: window.height < 750 ? 235 : 265
            radius: 7; clip: true
            gradient: Gradient { orientation: Gradient.Horizontal; GradientStop { position: 0; color: "#283338" } GradientStop { position: 1; color: "#1E282B" } }
            Rectangle { width: 310; height: 310; radius: 155; anchors.right: parent.right; anchors.rightMargin: -12; anchors.verticalCenter: parent.verticalCenter; color: "transparent"; border.width: 1; border.color: "#43524C"; opacity: .6 }
            Rectangle { width: 376; height: 376; radius: 188; anchors.right: parent.right; anchors.rightMargin: -45; anchors.verticalCenter: parent.verticalCenter; color: "transparent"; border.width: 1; border.color: "#43524C"; opacity: .24 }
            Image { source: window.isHoseGame ? "qrc:/assets/games/hose-for-hire-logo.png" : "qrc:/assets/brand/TeamHJD-Logo.png"; width: window.isHoseGame ? parent.width*.43 : Math.min(290, parent.width*.35); height: window.isHoseGame ? parent.height*.84 : width; anchors.right: parent.right; anchors.rightMargin: window.isHoseGame ? 18 : 25; anchors.verticalCenter: parent.verticalCenter; fillMode: Image.PreserveAspectFit; opacity: window.isHoseGame ? 1 : .46; mipmap: true; Accessible.name: window.isHoseGame ? "Hose for Hire 게임 로고" : "TeamHJD" }
            Column {
                anchors.left: parent.left; anchors.leftMargin: 30; anchors.verticalCenter: parent.verticalCenter
                width: parent.width * (window.isHoseGame ? .49 : .60); spacing: 15
                Label { text: "TEAMHJD  /  PLAYTEST"; color: "#E8C76A"; font.pixelSize: 10; font.bold: true; font.letterSpacing: 2 }
                Label { width: parent.width; text: v.name; color: "#F7F6EE"; font.family: "Segoe UI"; font.pixelSize: v.name.length > 24 ? 33 : 45; font.weight: Font.Bold; font.letterSpacing: -.8; wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight }
                Label { width: parent.width; text: v.description; color: "#C0CBCB"; font.pixelSize: 12; lineHeight: 1.45; wrapMode: Text.WordWrap; maximumLineCount: 3; elide: Text.ElideRight }
            }
            Rectangle { height: 3; width: 48; color: "#E8C76A"; anchors.left: parent.left; anchors.bottom: parent.bottom; anchors.leftMargin: 30 }
        }
        RowLayout {
            Layout.fillWidth: true; Layout.topMargin: 23; Layout.bottomMargin: v.busy ? 10 : 24; spacing: 18
            ColumnLayout {
                spacing: 7; Layout.fillWidth: true
                Label { text: v.status; Layout.fillWidth: true; color: "#E8EBE7"; font.pixelSize: 13; font.bold: true; wrapMode: Text.WordWrap; maximumLineCount: 2 }
                Caption {
                    text: v.busy ? v.transfer : window.gameUpdateReady ? "현재 " + v.installed + "  →  최신 " + v.latest : v.installed ? "설치 버전 " + v.installed + (v.updateAvailable ? "  ·  최신 " + v.latest : "") : v.latest ? "버전 " + v.latest + "  ·  " + v.size : "Windows 64-bit  ·  비공개 팀 테스트"
                    Layout.fillWidth: true; wrapMode: Text.WrapAnywhere; elide: Text.ElideRight; maximumLineCount: window.gameUpdateReady ? 2 : 1
                }
                Caption { visible: window.gameUpdateReady; text: "업데이트를 눌러 적용"; Layout.fillWidth: true; color: "#C5B580"; font.pixelSize: 11 }
            }
            ActionButton { text: "취소"; visible: v.busy && v.canCancel; onClicked: launcher.cancel() }
            ActionButton { text: "현재 버전 플레이"; visible: window.gameUpdateReady; implicitWidth: 148; implicitHeight: 50; enabled: !!v.canPlay; onClicked: launcher.playInstalled() }
            ActionButton { text: v.action; prominent: true; visible: !v.busy; implicitWidth: 164; implicitHeight: 50; font.pixelSize: 15; enabled: !v.running && !v.busy; onClicked: launcher.primary() }
            ActionButton { id: menuButton; text: "···"; padding: 0; implicitWidth: 40; implicitHeight: 50; font.pixelSize: 21; enabled: !v.busy; onClicked: gameMenu.open(); Accessible.name: "게임 관리" }
        }
        ProgressBar {
            id: progressBar
            Layout.fillWidth: true; Layout.preferredHeight: v.busy ? 4 : 0; Layout.bottomMargin: v.busy ? 23 : 0
            visible: v.busy; value: v.progress; from: 0; to: 1
            background: Rectangle { color: "#30383E"; radius: 2 }
            contentItem: Item { Rectangle { width: parent.width * progressBar.visualPosition; height: parent.height; radius: 2; color: "#E8C76A"; Behavior on width { NumberAnimation { duration: launcher.reduceMotion ? 0 : 100 } } } }
            Accessible.name: "설치 진행률"
        }
        Rectangle { Layout.fillWidth: true; height: 1; color: "#30373D" }
        RowLayout {
            Layout.fillWidth: true; Layout.topMargin: 22; Layout.bottomMargin: 14
            Label { text: "업데이트 소식"; color: "#EBEDE8"; font.pixelSize: 14; font.bold: true }
            Item { Layout.fillWidth: true }
            Caption { text: v.latest ? "BUILD  /  " + v.latest : "테스트 채널 연결 전"; color: "#8B979F"; font.pixelSize: 10 }
        }
        ScrollView {
            Layout.fillWidth: true; Layout.fillHeight: true; clip: true
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            TextArea { readOnly: true; text: v.notes; color: "#AEB8BF"; font.pixelSize: 12; wrapMode: Text.WordWrap; selectByMouse: true; textFormat: TextEdit.PlainText; padding: 0; rightPadding: 15; background: null; Accessible.name: "게임 변경사항" }
        }
        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: errorLabel.implicitHeight + 24; Layout.topMargin: 14; visible: v.error.length > 0
            color: "#382B28"; radius: 4; border.color: "#6E4C40"
            Label { id: errorLabel; anchors.left: parent.left; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; anchors.margins: 12; text: v.error; color: "#F0BAA2"; font.pixelSize: 12; wrapMode: Text.WordWrap; Accessible.role: Accessible.AlertMessage }
        }
        RowLayout {
            Layout.fillWidth: true; Layout.topMargin: 19
            Caption { text: "Happy Journey of Developers"; font.family: "Segoe UI"; font.pixelSize: 10; color: "#66747D" }
            Item { Layout.fillWidth: true }
            Caption { text: v.updateMessage; font.pixelSize: 10; color: "#849099" }
        }
    }

    AppMenu {
        id: gameMenu
        parent: menuButton
        x: menuButton.width-width
        y: menuButton.height + 8
        AppMenuItem { text: "버전 및 설치 정보"; onTriggered: gameInfo.open() }
        AppMenuItem { text: "설치 폴더 열기"; enabled: !!v.installed; onTriggered: launcher.openFolder("game") }
        AppMenuItem { text: "테스트 로그 열기"; onTriggered: launcher.openFolder("logs") }
        AppMenuSeparator {}
        AppMenuItem { text: "파일 복구"; enabled: v.online && !v.running && !v.busy; onTriggered: launcher.repair() }
        AppMenuItem { text: "이전 버전으로 복원"; enabled: v.previous && !v.running && !v.busy; onTriggered: launcher.rollback() }
        AppMenuSeparator {}
        AppMenuItem { text: "게임 삭제…"; destructive: true; enabled: v.canUninstall && !v.running && !v.busy; onTriggered: uninstallDialog.open() }
    }
    Popup {
        id: gameInfo; anchors.centerIn: Overlay.overlay; width: 650; height: Math.min(625,window.height-60); padding: 28; modal: true; focus: true
        Overlay.modal: Rectangle { color: "#B0000000" }
        background: Rectangle { color: "#20262B"; radius: 8; border.color: "#48525B" }
        contentItem: ColumnLayout {
            spacing: 18
            RowLayout { Layout.fillWidth: true; Label { text: "버전 및 설치 정보"; font.pixelSize: 20; font.bold: true; color: "#F0F0EA" } Item { Layout.fillWidth: true } ActionButton { text: "닫기"; quiet: true; implicitWidth: 64; onClicked: gameInfo.close() } }
            Label { text: v.name; font.pixelSize: 15; font.bold: true; color: "#E8C76A"; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            GridLayout { columns: 2; columnSpacing: 26; rowSpacing: 11; Layout.fillWidth: true
                Caption { text: "설치 버전" } Label { text: v.installed || "설치되지 않음"; color: "#ECF0EB"; font.pixelSize: 12 }
                Caption { text: "최신 배포" } Label { text: v.latest ? v.latest+(v.online ? "  ·  자동 확인됨" : "  ·  마지막 확인 정보") : "서버 연결 후 확인"; color: "#ECF0EB"; font.pixelSize: 12 }
                Caption { text: "이전 버전" } Label { text: v.previousVersion || "없음"; color: "#BEC9CE"; font.pixelSize: 12 }
                Caption { text: "사용 용량" } Label { text: v.canUninstall ? v.installedSize+"  ·  보관 버전 "+v.versionCount+"개 포함" : "—"; color: "#BEC9CE"; font.pixelSize: 12 }
                Caption { text: "설치한 날짜" } Label { text: v.installedAt ? Qt.formatDateTime(new Date(v.installedAt),"yyyy.MM.dd  hh:mm") : "—"; color: "#BEC9CE"; font.pixelSize: 12 }
            }
            Caption { text: v.folder || "게임을 설치하면 위치가 표시됩니다."; Layout.fillWidth: true; wrapMode: Text.WrapAnywhere }
            Rectangle { Layout.fillWidth: true; height: 1; color: "#3A434A" }
            Label { text: "최신 버전 변경사항"; color: "#E4EAE8"; font.bold: true; font.pixelSize: 13 }
            ScrollView { Layout.fillWidth: true; Layout.fillHeight: true; clip: true; TextArea { readOnly: true; text: v.notes; wrapMode: Text.WordWrap; textFormat: TextEdit.PlainText; color: "#AFBDC5"; font.pixelSize: 12; padding: 0; rightPadding: 16; background: null; selectByMouse: true } }
            ActionButton { text: "설치 폴더 열기"; enabled: !!v.installed; onClicked: launcher.openFolder("game") }
        }
    }
    Popup {
        id: uninstallDialog; anchors.centerIn: Overlay.overlay; width: 470; padding: 28; modal: true; focus: true
        Overlay.modal: Rectangle { color: "#B0000000" }
        background: Rectangle { color: "#252C31"; radius: 8; border.color: "#645149" }
        contentItem: ColumnLayout {
            spacing: 17
            Label { text: "게임을 삭제할까요?"; color: "#F0F0EA"; font.pixelSize: 20; font.bold: true }
            Label { text: v.name; color: "#E8C76A"; font.bold: true; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            Caption { text: "현재 설치와 보관 버전, 총 "+v.versionCount+"개를 삭제합니다. 사용 용량은 "+v.installedSize+"입니다.\n\n게임의 AppData 저장 데이터와 테스트 로그는 유지됩니다. 나중에 같은 라이브러리에서 다시 설치할 수 있어요."; Layout.fillWidth: true; lineHeight: 1.55 }
            RowLayout { Layout.fillWidth: true; Item { Layout.fillWidth: true } ActionButton { text: "취소"; onClicked: uninstallDialog.close() } ActionButton { text: "게임 삭제"; prominent: true; enabled: !v.busy&&!v.running; onClicked: {uninstallDialog.close();launcher.uninstall()} } }
        }
    }
    Popup {
        id: closeNotice; anchors.centerIn: Overlay.overlay; width: 410; padding: 24; modal: true; focus: true
        background: Rectangle { color: "#252B30"; radius: 8; border.color: "#485058" }
        contentItem: ColumnLayout { spacing: 20; Label { text: "작업을 마친 뒤 종료할 수 있어요"; color: "#F0F0EA"; font.bold: true } Caption { text: "다운로드나 설치 중에는 취소를 누른 뒤 창을 닫아 주세요."; Layout.fillWidth: true } ActionButton { text: "확인"; Layout.alignment: Qt.AlignRight; onClicked: closeNotice.close() } }
    }
    Popup {
        id: loginNotice
        anchors.centerIn: Overlay.overlay; width: Math.min(520,window.width-60)
        modal: true; focus: true; padding: 28; closePolicy: Popup.CloseOnEscape
        Overlay.modal: Rectangle { color: "#B0000000" }
        background: Rectangle { color: "#20262B"; radius: 9; border.color: "#48525B" }
        contentItem: ColumnLayout {
            spacing: 18
            Label { text: "이메일로 테스트 참여"; color: "#F0F0EA"; font.pixelSize: 21; font.bold: true }
            Caption { text: "브라우저에서 이메일로 받은 일회용 코드를 입력하면 연결됩니다. 인증을 마친 뒤 런처로 돌아오세요."; Layout.fillWidth: true; lineHeight: 1.5 }
            Rectangle { Layout.fillWidth: true; implicitHeight: privacyText.implicitHeight+28; radius: 5; color: "#171C20"; border.color: "#333D44"
                Caption { id: privacyText; anchors.fill: parent; anchors.margins: 14; text: "TeamHJD는 테스트 운영을 위해 인증된 이메일, 최초·최근 인증 시각, 최근 접속 시각과 인증 횟수를 비공개 서버에 저장합니다. 이메일 주소별 참여 제한은 없습니다."; lineHeight: 1.55 }
            }
            Caption { text: "연결 정보는 이 PC의 Windows 계정으로 암호화해 보관합니다. 인증은 Cloudflare Access에서 처리합니다."; Layout.fillWidth: true; lineHeight: 1.5 }
            RowLayout { Layout.fillWidth: true; Item {Layout.fillWidth: true} ActionButton { text: "취소"; onClicked: loginNotice.close() } ActionButton { text: "브라우저에서 인증"; prominent: true; onClicked: {loginNotice.close();launcher.login()} } }
        }
    }
    Popup {
        id: settings
        anchors.centerIn: Overlay.overlay; width: Math.min(620, window.width-60); height: Math.min(710, window.height-40)
        modal: true; focus: true; padding: 28
        closePolicy: Popup.CloseOnEscape
        Overlay.modal: Rectangle { color: "#B0000000" }
        background: Rectangle { color: "#20262B"; radius: 9; border.color: "#48525B" }
        onOpened: { serverField.text=launcher.serverUrl; keyField.text=launcher.publisherKey; locationField.text=launcher.installRoot; passwordField.text=""; motionCheck.checked=launcher.reduceMotion; window.advancedSettings=!v.cloudflare;locationField.forceActiveFocus() }
        contentItem: ColumnLayout {
            spacing: 0
            RowLayout { Layout.fillWidth: true; Label { text: "런처 설정"; font.pixelSize: 22; font.bold: true; color: "#F0F0EA" } Item { Layout.fillWidth: true } ActionButton { text: "닫기"; quiet: true; implicitWidth: 66; onClicked: settings.close() } }
            Caption { text: "이메일로 연결하고 설치 위치와 화면 옵션을 설정하세요."; Layout.topMargin: 10; Layout.bottomMargin: 22; Layout.fillWidth: true }
            ScrollView {
                id: formScroll; Layout.fillWidth: true; Layout.fillHeight: true; clip: true; ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                ColumnLayout {
                    width: formScroll.availableWidth - 14; spacing: 10
                    ActionButton { text: v.needsLogin ? "이메일로 연결" : "이메일 다시 인증"; visible: v.cloudflare; prominent: true; Layout.fillWidth: true; enabled: !v.working; onClicked: {settings.close();loginNotice.open()} }
                    Caption { text: v.needsLogin ? "브라우저에서 이메일 일회용 코드로 인증합니다." : "연결됨 · 인증 정보는 이 PC의 Windows 계정으로 보호됩니다."; visible: v.cloudflare; Layout.fillWidth: true }
                    AppCheckBox { text: "고급 연결 설정"; checked: window.advancedSettings; onToggled: window.advancedSettings=checked }
                    ActionButton { text: "초대 파일로 연결"; visible: window.advancedSettings; Layout.fillWidth: true; onClicked: inviteDialog.open() }
                    Label { visible: window.advancedSettings; text: "테스트 서버"; color: "#E0E5E5"; Layout.topMargin: 14 }
                    Field { id: serverField; visible: window.advancedSettings; Layout.fillWidth: true; placeholderText: "https://서버주소:포트/"; inputMethodHints: Qt.ImhUrlCharactersOnly; Accessible.name: "테스트 서버 주소" }
                    Label { visible: window.advancedSettings; text: "접근 암호"; color: "#E0E5E5"; Layout.topMargin: 8 }
                    Field { id: passwordField; visible: window.advancedSettings; Layout.fillWidth: true; placeholderText: launcher.serverUrl ? "변경할 때만 입력" : "팀에서 전달받은 암호"; echoMode: TextInput.Password; Accessible.name: "배포 접근 암호" }
                    Caption { visible: window.advancedSettings; text: "암호는 이 PC의 Windows 계정으로 보호하여 저장합니다."; Layout.fillWidth: true }
                    Label { visible: window.advancedSettings; text: "배포자 키"; color: "#E0E5E5"; Layout.topMargin: 8 }
                    Field { id: keyField; visible: window.advancedSettings; Layout.fillWidth: true; placeholderText: "p256:…  (초대 파일에 포함)"; font.pixelSize: 11; Accessible.name: "배포자 공개키" }
                    Caption { visible: window.advancedSettings; text: "게임의 출처를 확인하는 공개키입니다. 팀이 전달한 값과 일치해야 합니다."; Layout.fillWidth: true }
                    Label { text: "게임 설치 폴더"; color: "#E0E5E5"; Layout.topMargin: 8 }
                    RowLayout { Layout.fillWidth: true; Field { id: locationField; Layout.fillWidth: true; placeholderText: "게임 설치 폴더의 전체 경로" } ActionButton { text: "찾기"; implicitWidth: 70; onClicked: folderDialog.open() } }
                    Caption { text: "위치를 바꾸면 새 설치부터 적용됩니다. 기존 게임 파일과 저장 데이터는 유지됩니다."; Layout.fillWidth: true }
                    AppCheckBox { id: motionCheck; text: "화면 움직임 줄이기"; Layout.topMargin: 12 }
                }
            }
            Label { Layout.fillWidth: true; Layout.topMargin: 14; visible: v.error.length > 0; text: v.error; color: "#F0BAA2"; wrapMode: Text.WordWrap; font.pixelSize: 12 }
            ActionButton { text: "저장하고 연결"; prominent: true; Layout.fillWidth: true; Layout.topMargin: 20; enabled: !v.working; onClicked: if(launcher.saveSettings(serverField.text,passwordField.text,keyField.text,locationField.text,motionCheck.checked)) settings.close() }
        }
    }
    FileDialog { id: inviteDialog; title: "TeamHJD 초대 파일 선택"; nameFilters: ["TeamHJD 초대 (*.hjdinvite *.json)"]; onAccepted: window.connectInvite(selectedFile) }
    FolderDialog { id: folderDialog; title: "게임 설치 폴더 선택"; onAccepted: locationField.text=launcher.localPath(selectedFolder) }

    Popup {
        id: notificationPanel; x: window.width-width-16; y: titlebar.height+8; width: 370; height: Math.min(480,window.height-80); padding: 20; focus: true
        background: Rectangle { color: "#252C31"; radius: 7; border.color: "#48525B" }
        contentItem: ColumnLayout {
            spacing: 16
            RowLayout { Layout.fillWidth: true; Label { text: "알림"; color: "#EFF0EB"; font.pixelSize: 17; font.bold: true } Item { Layout.fillWidth: true } ActionButton { text: "닫기"; quiet: true; implicitWidth: 58; implicitHeight: 30; onClicked: notificationPanel.close() } }
            Caption { visible: notificationHistory.count===0; text: "아직 새로운 알림이 없어요.\n연결과 업데이트 소식을 여기에서 확인하세요."; Layout.fillWidth: true; lineHeight: 1.6 }
            ListView { Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 17; model: notificationHistory
                delegate: Column {
                    required property string noticeTitle; required property string noticeBody; required property string time
                    width: ListView.view.width; spacing: 6
                    Label { text: parent.noticeTitle; width: parent.width; wrapMode: Text.WordWrap; font.bold: true; color: "#ECEFEA"; font.pixelSize: 12 }
                    Caption { text: parent.noticeBody; width: parent.width; lineHeight: 1.4 }
                    Caption { text: parent.time; font.pixelSize: 10; color: "#7D8A93" }
                }
            }
        }
    }
    Column {
        id: toastStack; z: 50; anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 24; width: 365; spacing: 10
        Repeater { model: toastModel
            delegate: Rectangle {
                id: toastCard
                required property string noticeTitle; required property string noticeBody; required property string noticeTone; required property int uid
                width: toastStack.width; height: toastContents.implicitHeight+28; radius: 6; color: "#2A3238"; border.color: "#4C5961"
                Accessible.role: Accessible.AlertMessage; Accessible.name: noticeTitle+" "+noticeBody
                Rectangle { width: 3; anchors.top: parent.top; anchors.bottom: parent.bottom; anchors.margins: 9; color: toastCard.noticeTone==="error" ? "#ED9B7E" : toastCard.noticeTone==="success" ? "#8CBD9E" : "#E8C76A" }
                Column { id: toastContents; anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.leftMargin: 19; anchors.rightMargin: 38; anchors.topMargin: 14; spacing: 7
                    Label { text: toastCard.noticeTitle; width: parent.width; color: "#F1F2EC"; font.bold: true; font.pixelSize: 12; wrapMode: Text.WordWrap }
                    Caption { text: toastCard.noticeBody; width: parent.width; lineHeight: 1.45; visible: text.length>0 }
                }
                ToolButton { id: dismissToast; anchors.right: parent.right; anchors.top: parent.top; width: 32; height: 32; Accessible.name: "알림 닫기"; onClicked: window.removeToast(toastCard.uid); contentItem: Text { text: "×"; color: "#A6B1B8"; font.pixelSize: 19; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter } background: Rectangle { color: dismissToast.hovered ? "#3B444B" : "transparent"; radius: 4; border.width: dismissToast.visualFocus ? 1 : 0; border.color: "#C7B98E" } }
                Timer { interval: toastCard.noticeTone==="error" ? 8000 : 5500; running: true; onTriggered: window.removeToast(toastCard.uid) }
                opacity: 1
                Component.onCompleted: { if(!launcher.reduceMotion) toastEnter.start() }
                NumberAnimation { id: toastEnter; target: toastCard; property: "opacity"; from: 0; to: 1; duration: 150 }
            }
        }
    }
    Popup {
        id: helpDialog; anchors.centerIn: Overlay.overlay; width: 640; height: Math.min(575,window.height-60); modal: true; focus: true; padding: 28
        Overlay.modal: Rectangle { color: "#B0000000" }
        background: Rectangle { color: "#20262B"; radius: 8; border.color: "#48525B" }
        contentItem: ColumnLayout {
            spacing: 20
            RowLayout { Layout.fillWidth: true; Label { text: "도움말"; color: "#F0F0EA"; font.pixelSize: 22; font.bold: true } Item { Layout.fillWidth: true } ActionButton { text: "닫기"; quiet: true; implicitWidth: 65; onClicked: helpDialog.close() } }
            TabBar { id: helpTabs; Layout.fillWidth: true; palette.button: "#2D353C"; palette.buttonText: "#DFE5E5"; palette.highlight: "#E8C76A"; HelpTab { text: "시작하기" } HelpTab { text: "문제 해결" } HelpTab { text: "런처 정보" } }
            ScrollView { Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                TextArea { readOnly: true; wrapMode: Text.WordWrap; selectByMouse: true; textFormat: TextEdit.PlainText; color: "#C4CED3"; font.pixelSize: 13; padding: 0; rightPadding: 14; background: null
                    text: helpTabs.currentIndex===0 ? "1. 이메일로 시작\n이메일로 시작을 누르고 브라우저에서 이메일과 일회용 코드를 입력하세요. 인증을 마치면 런처로 돌아오세요. 별도 초대 파일이 전달된 경우 설정에서 가져올 수도 있습니다.\n\n2. 설치하고 플레이\n게임의 설치 버튼을 누르면 다운로드와 파일 확인이 이어집니다. 준비가 끝나면 플레이를 선택하세요.\n\n3. 새 버전은 직접 업데이트\n연결과 최신 버전은 자동 확인합니다. 게임의 새 버전은 업데이트를 눌러 다운로드하고 적용하세요. 현재 버전 플레이로 기존 버전을 계속 실행할 수도 있습니다. 게임 실행 중이라면 종료한 뒤 업데이트를 눌러 주세요. 런처의 새 버전은 자동으로 준비해 다음 실행에 적용됩니다.\n\n단축키\nCtrl+,  설정   ·   Ctrl+B  사이드바   ·   F1  도움말" : helpTabs.currentIndex===1 ? "서버에 연결할 수 없어요\n인터넷을 확인하세요. 이메일 인증이 만료되면 파일 메뉴 또는 설정에서 이메일로 다시 연결하세요. 연결이 돌아오면 자동으로 다시 확인합니다. 설치된 버전은 오프라인에서도 플레이할 수 있습니다.\n\n인증 정보와 개인정보\n인증된 이메일, 최초·최근 인증 시각과 최근 접속 시각은 TeamHJD 운영 서버에 저장됩니다. 게임 로그는 자동 전송하지 않습니다. 초대 파일을 사용하는 경우 팀에서 새 초대를 받을 수 있습니다.\n\n설치가 중단됐어요\n설치를 다시 선택하면 이어받습니다. 기존 정상 버전은 유지됩니다.\n\n파일 문제 또는 새 버전 문제\n게임 관리 ··· 메뉴에서 파일 복구 또는 이전 버전 복원을 사용할 수 있습니다. 게임을 먼저 종료하세요.\n\n테스트 로그\n게임 관리에서 로그 폴더를 열 수 있습니다. 로그를 자동으로 외부에 전송하지 않습니다." : "TeamHJD Game Test Launcher\nv"+launcher.version+"  ·  Windows x64\n\nHappy Journey of Developers\n팀의 다음 플레이테스트를 위한 게임 라이브러리입니다.\n\nC++ · Qt 6.8.3\nQt is used under LGPLv3. Copyright The Qt Company Ltd. and other contributors.\n라이브러리 라이선스와 소스 안내는 함께 제공되는 THIRD_PARTY_NOTICES.md 및 licenses 폴더에 있습니다.\n\n공개 런처 업데이트와 비공개 게임 배포는 분리됩니다. 게임과 접근 암호는 공개 Release에 포함되지 않습니다."
                }
            }
            ActionButton { visible: helpTabs.currentIndex===2; text: "오픈소스 고지 열기"; onClicked: launcher.showLicenses(); Layout.alignment: Qt.AlignLeft }
        }
    }
}
