pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import Holonight.Core
import Holonight.Controls
import Holonight.FileBrowser
import Holonight.FileBrowser.Preview
import HolonightPortal.Private

HnApplicationWindow {
    id: root
    required property Controller controller
    readonly property int heightLimit: Math.max(1, Math.min(600, Screen.desktopAvailableHeight - 48))
    flags: Qt.Dialog
    property bool editingLocation: false
    property bool compactPreview: false
    readonly property bool previewVisible: width >= 900 || compactPreview
    width: Math.max(600, Math.min(1160, Screen.desktopAvailableWidth - 48))
    height: heightLimit
    maximumHeight: heightLimit
    minimumWidth: 600
    minimumHeight: Math.min(400, heightLimit)
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

    function editLocation(): void {
        editingLocation = true;
        location.forceActiveFocus();
        location.selectAll();
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12
        HnPanelHeader {
            objectName: "purpose"
            Layout.fillWidth: true
            title: root.controller.save ? qsTr("Save a file") : root.controller.directoryMode ? (root.controller.multiple ? qsTr("Choose folders") : qsTr("Choose a folder")) : (root.controller.multiple ? qsTr("Choose files") : qsTr("Choose a file"))
        }
        RowLayout {
            Layout.fillWidth: true
            Controls.Button {
                text: qsTr("Up")
                onClicked: root.controller.parentFolder()
            }
            HnBreadcrumbBar {
                Layout.fillWidth: true
                visible: !root.editingLocation
                segments: root.controller.breadcrumbLabels
                onSegmentActivated: index => root.controller.activateBreadcrumb(index)
                onLocationEditRequested: root.editLocation()
            }
            Controls.TextField {
                id: location
                visible: root.editingLocation
                objectName: "location"
                Layout.fillWidth: true
                text: root.controller.location
                Accessible.name: qsTr("Location")
                onAccepted: {
                    root.controller.navigate(text);
                    root.editingLocation = false;
                    listing.forceActiveFocus();
                }
            }
            Controls.CheckBox {
                objectName: "showHidden"
                text: qsTr("Show hidden files")
                checked: root.controller.hiddenVisible
                onClicked: root.controller.hiddenVisible = checked
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            ColumnLayout {
                Layout.preferredWidth: 170
                Layout.maximumWidth: 170
                Layout.fillHeight: true
                HnLabel {
                    role: HnTypographyRole.MicroHeader
                    rawText: qsTr("Places")
                }
                ListView {
                    id: places
                    objectName: "places"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 0
                    clip: true
                    model: root.controller.places
                    Controls.ScrollBar.vertical: Controls.ScrollBar {}
                    delegate: HnNavigationDelegate {
                        id: place
                        required property var modelData
                        width: places.width
                        sizeRole: HnControlSize.Compact
                        title: modelData.name
                        highlighted: modelData.path === root.controller.location
                        leadingContent: Component {
                            HnIcon {
                                size: HnMetrics.controlHeight(HnControlSize.Compact) / 2
                                source: place.modelData.iconName ? "image://icon/" + place.modelData.iconName : ""
                                rendering: HnIcon.Original
                            }
                        }
                        onClicked: {
                            root.controller.navigate(modelData.path);
                            listing.forceActiveFocus();
                        }
                    }
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                HnSearchField {
                    id: query
                    objectName: "search"
                    Layout.fillWidth: true
                    placeholderText: qsTr("Search this folder")
                    Accessible.name: qsTr("Search this folder")
                    onTextChanged: root.controller.search(text)
                    onAccepted: listing.forceActiveFocus()
                }
                DetailedColumnLayout {
                    id: detailColumns
                    availableWidth: listing.width
                    selectionWidth: root.controller.multiple ? 36 : 0
                }
                DetailedListHeader {
                    Layout.fillWidth: true
                    columns: detailColumns
                }
                DirectoryListing {
                    id: listing
                    objectName: "listing"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 0
                    model: root.controller.entries
                    currentIndex: root.controller.cursor
                    onCursorRequested: row => {
                        root.controller.select(row);
                        listing.forceActiveFocus();
                    }
                    onActivationRequested: row => root.controller.activate(row)
                    onSelectionToggled: row => root.controller.toggle(row)
                    delegate: DetailedListRow {
                        id: entry
                        required property int index
                        required property string name
                        required property bool isDir
                        required property bool isParent
                        required property bool statFailed
                        required property string statError
                        required property string nativeId
                        required property string iconName
                        required property real size
                        required property var modified
                        width: listing.width
                        columns: detailColumns
                        title: name
                        filename: name
                        sizeText: isDir ? "" : Number(size).toLocaleString(Qt.locale())
                        modifiedText: isParent ? "" : modified.toLocaleString(Qt.locale(), Locale.ShortFormat)
                        highlighted: index === root.controller.cursor || root.controller.selectedIds.indexOf(nativeId) >= 0
                        selectionContent: Controls.CheckBox {
                            objectName: "selection-" + entry.nativeId
                            enabled: !entry.isParent && !entry.statFailed && entry.isDir === root.controller.directoryMode
                            checked: root.controller.selectedIds.indexOf(entry.nativeId) >= 0
                            Accessible.name: qsTr("Select %1").arg(entry.name)
                            onClicked: {
                                root.controller.toggle(entry.index);
                                listing.forceActiveFocus();
                            }
                        }
                        filenameContent: RowLayout {
                            HnIcon {
                                size: HnMetrics.controlHeight(HnControlSize.Compact)
                                source: entry.iconName ? "image://icon/" + entry.iconName : ""
                                rendering: HnIcon.Original
                            }
                            HnLabel {
                                rawText: entry.name
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                        }
                        Keys.onSpacePressed: root.controller.toggle(index)
                        onClicked: {
                            root.controller.select(index);
                            listing.forceActiveFocus();
                        }
                        onDoubleClicked: root.controller.activate(index)
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
            FilePreview {
                objectName: "entryPreview"
                visible: root.previewVisible
                Layout.preferredWidth: Math.min(300, root.width * 0.32)
                Layout.fillHeight: true
                service: root.controller.preview
                metadataRows: service.hasEntry ? [
                    {
                        label: qsTr("Name"),
                        value: service.name
                    },
                    {
                        label: qsTr("Size"),
                        value: Number(service.size).toLocaleString(Qt.locale())
                    },
                    {
                        label: qsTr("Modified"),
                        value: service.modified.toLocaleString(Qt.locale(), Locale.ShortFormat)
                    },
                    {
                        label: qsTr("Type"),
                        value: service.mimeTypeDescription
                    },
                    {
                        label: qsTr("Permissions"),
                        value: service.permissions
                    },
                    {
                        label: qsTr("Dimensions"),
                        value: service.sourcePixelSize.width > 0 ? "%1 × %2".arg(service.sourcePixelSize.width).arg(service.sourcePixelSize.height) : ""
                    },
                    {
                        label: qsTr("Camera"),
                        value: [service.exifMake, service.exifModel].filter(part => part.length > 0).join(" ")
                    },
                    {
                        label: qsTr("Lens"),
                        value: service.exifLensModel
                    },
                    {
                        label: qsTr("Exposure"),
                        value: service.exifExposureTime
                    },
                    {
                        label: qsTr("ISO"),
                        value: service.exifIso
                    }
                ].filter(row => String(row.value).length > 0) : []
            }
        }
        Controls.ToolButton {
            objectName: "previewToggle"
            visible: root.width < 900
            text: root.compactPreview ? qsTr("Hide preview") : qsTr("Show preview")
            Accessible.name: text
            onClicked: root.compactPreview = !root.compactPreview
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
        HnEmptyState {
            Layout.fillWidth: true
            titleText: root.controller.explanation
            visible: titleText.length > 0
        }
        HnActionBar {
            Layout.fillWidth: true
            leadingContent: RowLayout {
                HnLabel {
                    role: HnTypographyRole.Caption
                    visible: root.controller.multiple
                    rawText: qsTr("%n selected", "", root.controller.selectedIds.length)
                }
                Controls.BusyIndicator {
                    running: root.controller.busy
                }
            }
            trailingContent: RowLayout {

                Controls.Button {
                    objectName: "cancel"
                    text: qsTr("Cancel")
                    onClicked: root.controller.cancel()
                }
                Controls.Button {
                    objectName: "accept"
                    text: root.controller.acceptLabel
                    highlighted: true
                    enabled: !root.controller.busy && !root.controller.overwritePending
                    onClicked: {
                        root.controller.accept();
                        if (!root.controller.save)
                            listing.forceActiveFocus();
                    }
                }
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
            root.editLocation();
        }
    }
    Shortcut {
        sequence: "Ctrl+H"
        enabled: !root.controller.overwritePending
        onActivated: root.controller.hiddenVisible = !root.controller.hiddenVisible
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
