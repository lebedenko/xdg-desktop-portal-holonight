pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import Holonight.Core
import Holonight.Controls
import Holonight.FileBrowser
import HolonightPortal.Private

HnApplicationWindow {
    id: root
    required property Controller controller
    width: 820
    height: 600
    minimumWidth: 600
    minimumHeight: 400
    title: controller.title
    onClosing: event => {
        event.accepted = false;
        controller.cancel();
    }
    Component.onCompleted: {
        if (controller.save)
            filename.forceActiveFocus();
        else
            listing.forceActiveFocus();
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12
        RowLayout {
            Layout.fillWidth: true
            Controls.Button {
                text: qsTr("Up")
                onClicked: root.controller.parentFolder()
            }
            Controls.TextField {
                id: location
                objectName: "location"
                Layout.fillWidth: true
                text: root.controller.location
                Accessible.name: qsTr("Location")
                onAccepted: {
                    root.controller.navigate(text);
                    listing.forceActiveFocus();
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            ColumnLayout {
                Layout.preferredWidth: 150
                Layout.alignment: Qt.AlignTop
                Repeater {
                    model: root.controller.places
                    Controls.Button {
                        required property var modelData
                        Layout.fillWidth: true
                        text: modelData.name
                        onClicked: root.controller.navigate(modelData.path)
                    }
                }
                Item {
                    Layout.fillHeight: true
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Controls.TextField {
                    id: query
                    objectName: "search"
                    Layout.fillWidth: true
                    placeholderText: qsTr("Search this folder")
                    Accessible.name: qsTr("Search this folder")
                    onTextEdited: root.controller.search(text)
                    onAccepted: listing.forceActiveFocus()
                }
                DirectoryListing {
                    id: listing
                    objectName: "listing"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    model: root.controller.entries
                    currentIndex: root.controller.cursor
                    onCursorRequested: row => root.controller.cursor = row
                    onActivationRequested: row => root.controller.activate(row)
                    onSelectionToggled: row => root.controller.toggle(row)
                    delegate: RowLayout {
                        id: entry
                        required property int index
                        required property string name
                        required property bool isDir
                        required property bool isParent
                        required property bool statFailed
                        required property string statError
                        required property string nativeId
                        required property string iconName
                        width: listing.width
                        spacing: 4
                        Controls.CheckBox {
                            objectName: "selection-" + entry.nativeId
                            visible: root.controller.multiple
                            enabled: !entry.isParent && !entry.statFailed && entry.isDir === root.controller.directoryMode
                            // Controller exposes exact native identity independently of display text.
                            checked: root.controller.selectedIds.indexOf(entry.nativeId) >= 0
                            Accessible.name: qsTr("Select %1").arg(entry.name)
                            onClicked: root.controller.toggle(entry.index)
                        }
                        HnListDelegate {
                            Layout.fillWidth: true
                            title: entry.name
                            leadingContent: fileIcon
                            subtitle: entry.statFailed ? entry.statError : (entry.isDir ? qsTr("Folder") : "")
                            highlighted: entry.index === root.controller.cursor || root.controller.selectedIds.indexOf(entry.nativeId) >= 0
                            onClicked: root.controller.cursor = entry.index
                            onDoubleClicked: root.controller.activate(entry.index)
                            Component {
                                id: fileIcon
                                HnIcon {
                                    size: HnMetrics.controlHeight(HnControlSize.Compact)
                                    source: "image://icon/" + entry.iconName
                                    rendering: HnIcon.Original
                                }
                            }
                        }
                    }
                    Keys.onPressed: event => {
                        if (event.key === Qt.Key_J || event.key === Qt.Key_Down)
                            root.controller.move(1);
                        else if (event.key === Qt.Key_K || event.key === Qt.Key_Up)
                            root.controller.move(-1);
                        else if (event.key === Qt.Key_H || event.key === Qt.Key_Left)
                            root.controller.parentFolder();
                        else if (event.key === Qt.Key_L || event.key === Qt.Key_Right)
                            root.controller.activate(root.controller.cursor);
                        else if (event.key === Qt.Key_Space)
                            root.controller.toggle(root.controller.cursor);
                        else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                            if (root.controller.cursorIsDirectory())
                                root.controller.activate(root.controller.cursor);
                            else
                                root.controller.accept();
                        } else
                            return;
                        event.accepted = true;
                    }
                }
            }
        }
        RowLayout {
            visible: root.controller.save
            Layout.fillWidth: true
            Controls.Label {
                text: qsTr("Filename")
            }
            Controls.TextField {
                id: filename
                objectName: "filename"
                Layout.fillWidth: true
                text: root.controller.filename
                Accessible.name: qsTr("Filename")
                onTextEdited: root.controller.filename = text
                onAccepted: root.controller.accept()
            }
        }
        Controls.ComboBox {
            objectName: "filter"
            visible: root.controller.filterNames.length > 0
            Layout.fillWidth: true
            model: root.controller.filterNames
            currentIndex: root.controller.filterIndex
            Accessible.name: qsTr("File type")
            onActivated: index => root.controller.filterIndex = index
        }
        Repeater {
            model: root.controller.choices
            RowLayout {
                id: choice
                required property int index
                required property var modelData
                Layout.fillWidth: true
                Controls.Label {
                    text: choice.modelData.label
                }
                Controls.CheckBox {
                    visible: choice.modelData.items.length === 0
                    checked: choice.modelData.selected === "true"
                    Accessible.name: choice.modelData.label
                    onClicked: root.controller.setChoice(choice.index, checked ? "true" : "false")
                }
                Controls.ComboBox {
                    visible: choice.modelData.items.length > 0
                    Layout.fillWidth: true
                    model: choice.modelData.items
                    textRole: "label"
                    valueRole: "id"
                    currentIndex: indexOfValue(choice.modelData.selected)
                    Accessible.name: choice.modelData.label
                    onActivated: root.controller.setChoice(choice.index, currentValue)
                }
            }
        }
        Controls.Label {
            Layout.fillWidth: true
            text: root.controller.explanation
            wrapMode: Text.Wrap
            visible: text.length > 0
        }
        RowLayout {
            Layout.fillWidth: true
            Controls.BusyIndicator {
                running: root.controller.busy
            }
            Item {
                Layout.fillWidth: true
            }
            Controls.Button {
                text: qsTr("Cancel")
                onClicked: root.controller.cancel()
            }
            Controls.Button {
                objectName: "accept"
                text: root.controller.acceptLabel
                enabled: !root.controller.busy && !root.controller.overwritePending
                onClicked: root.controller.accept()
            }
        }
    }
    Controls.Dialog {
        id: overwrite
        objectName: "overwrite"
        title: qsTr("Replace existing file?")
        modal: true
        anchors.centerIn: parent
        standardButtons: Controls.Dialog.Yes | Controls.Dialog.No
        visible: root.controller.overwritePending
        onAccepted: root.controller.confirmOverwrite(true)
        onRejected: root.controller.confirmOverwrite(false)
        Controls.Label {
            text: qsTr("The destination exists. Confirm replacement before saving.")
        }
    }
    Shortcut {
        sequence: "Ctrl+L"
        enabled: !root.controller.overwritePending
        onActivated: {
            location.forceActiveFocus();
            location.selectAll();
        }
    }
    Shortcut {
        sequence: "/"
        enabled: listing.activeFocus
        onActivated: query.forceActiveFocus()
    }
    Shortcut {
        sequence: "Escape"
        enabled: !root.controller.overwritePending
        onActivated: root.controller.cancel()
    }
    Connections {
        target: root.controller
        function onQueryChanged() {
            if (query.text.length > 0 && query.text !== root.controller.query)
                query.text = root.controller.query;
        }
    }
}
