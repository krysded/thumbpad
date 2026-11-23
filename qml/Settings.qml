import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Shown as a layer-shell panel above the keyboard (see settingswindow.cpp):
// it never takes focus, so the keyboard stays visible and every change is
// applied live under your thumbs.
Window {
    id: root
    visible: false
    width: 640
    height: 600
    color: "transparent"
    title: "thumbpad settings"

    Material.theme: Material.Dark
    Material.accent: "#4c8dff"

    // Re-evaluate bindings whenever any setting changes.
    function val(key) { return settings.revision, settings.get(key) }

    component SettingSlider: ColumnLayout {
        id: s
        required property string key
        required property string text
        property real from
        property real to
        property real step: 1
        property string unit: ""
        property int decimals: 0
        Layout.fillWidth: true
        spacing: 0
        RowLayout {
            Layout.fillWidth: true
            Label { text: s.text; font.pixelSize: 18; Layout.fillWidth: true }
            Label {
                text: Number(root.val(s.key)).toFixed(s.decimals) + s.unit
                font.pixelSize: 18
                color: settings.revision, settings.isOverridden(s.key) ? "#ffd166" : "#c8c8c8"
            }
        }
        Slider {
            Layout.fillWidth: true
            from: s.from; to: s.to; stepSize: s.step
            snapMode: Slider.SnapAlways
            value: root.val(s.key)
            onMoved: settings.set(s.key, value)
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: 18
        color: "#1e1f24"
        border.color: "#3a3d46"
        border.width: 1

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                Label { text: "thumbpad"; font.pixelSize: 24; font.bold: true; Layout.fillWidth: true }
                Button { text: "Close"; font.pixelSize: 18; onClicked: root.close() }
            }

            Flickable {
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentHeight: content.implicitHeight
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                ColumnLayout {
                    id: content
                    width: parent.width - 16
                    spacing: 6

                    Label { text: "Size and placement"; font.pixelSize: 15; color: "#4c8dff"; Layout.topMargin: 6 }
                    SettingSlider { key: "keyMm"; text: "Key size"; from: 10; to: 22; step: 0.5; decimals: 1; unit: " mm" }
                    SettingSlider { key: "gapMm"; text: "Gap between keys"; from: 0; to: 3; step: 0.1; decimals: 1; unit: " mm" }
                    SettingSlider { key: "opacity"; text: "Opacity"; from: 0.3; to: 1; step: 0.05; decimals: 2 }
                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            text: "Reserve space (windows shrink above the keyboard)"
                            font.pixelSize: 18; wrapMode: Text.Wrap; Layout.fillWidth: true
                        }
                        Switch { checked: keyboard.reserveSpace; onToggled: keyboard.reserveSpace = checked }
                    }
                    Button {
                        text: "Reset keyboard position"; font.pixelSize: 17
                        onClicked: keyboard.resetPositions()
                    }

                    Label { text: "Touch"; font.pixelSize: 15; color: "#4c8dff"; Layout.topMargin: 12 }
                    SettingSlider { key: "swipeThresholdMm"; text: "Swipe distance (shorter = tap)"; from: 1.5; to: 8; step: 0.25; decimals: 2; unit: " mm" }
                    SettingSlider { key: "longPressMs"; text: "Long-press delay"; from: 200; to: 1000; step: 25; unit: " ms" }
                    SettingSlider { key: "repeatDelayMs"; text: "Key repeat starts after"; from: 150; to: 1000; step: 25; unit: " ms" }
                    SettingSlider { key: "repeatIntervalMs"; text: "Key repeat interval"; from: 20; to: 200; step: 5; unit: " ms" }
                    SettingSlider { key: "slideStepMm"; text: "Cursor slide: distance per character"; from: 1; to: 6; step: 0.25; decimals: 2; unit: " mm" }

                    Label { text: "Behaviour"; font.pixelSize: 15; color: "#4c8dff"; Layout.topMargin: 12 }
                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            text: "Show the ⌨ tab while hidden"
                            font.pixelSize: 18; Layout.fillWidth: true
                        }
                        Switch {
                            checked: root.val("showHandle")
                            onToggled: settings.set("showHandle", checked)
                        }
                    }
                    Label {
                        visible: !root.val("showHandle")
                        text: "Without the tab, a hidden keyboard only comes back when you focus a new text field."
                        color: "#ffd166"; font.pixelSize: 15; wrapMode: Text.Wrap; Layout.fillWidth: true
                    }
                    Label { text: "Layout"; font.pixelSize: 18 }
                    ComboBox {
                        Layout.fillWidth: true
                        font.pixelSize: 16
                        model: ["Automatic"].concat(settings.layouts)
                        currentIndex: settings.layout === "" ? 0 : settings.layouts.indexOf(settings.layout) + 1
                        onActivated: (i) => settings.layout = i === 0 ? "" : settings.layouts[i - 1]
                    }

                    Button {
                        Layout.topMargin: 16
                        text: "Reset all to layout defaults"; font.pixelSize: 17
                        onClicked: settings.resetDefaults()
                    }
                    Label {
                        text: "Values in yellow are your own; the rest come from the layout file."
                        color: "#9aa3b2"; font.pixelSize: 14; wrapMode: Text.Wrap; Layout.fillWidth: true
                    }
                }
            }
        }
    }
}
