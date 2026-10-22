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
    readonly property int sidebarWidth: 220
    readonly property real panelInset: HnMetrics.internalSpacing(HnControlSize.Normal)
    readonly property bool wide: width >= 900
    onWideChanged: previewDrawer.close()
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
        spacing: 0
        Rectangle {
            objectName: "purposeSurface"
            color: HoloniightPalette.surface
            Layout.fillWidth: true
            implicitHeight: purpose.implicitHeight + 2 * root.panelInset
            HnPanelHeader {
                id: purpose
                anchors.fill: parent
                anchors.margins: root.panelInset
                objectName: "purpose"
                Layout.fillWidth: true
                title: root.controller.saveMany ? qsTr("Save Files") : root.controller.save ? qsTr("Save File") : root.controller.directoryMode ? (root.controller.multiple ? qsTr("Select Folders") : qsTr("Select Folder")) : (root.controller.multiple ? qsTr("Open Files") : qsTr("Open File"))
                description: root.controller.title.length > 0 ? root.controller.title : root.controller.save ? qsTr("Choose a location and name for your file.") : root.controller.saveMany ? qsTr("Choose a destination for your files.") : qsTr("Navigate, search, and select.")
                dividerVisible: false
                leadingContent: HnIcon {
                    size: 32
                    source: "image://icon/" + (root.controller.save || root.controller.saveMany ? "document-save" : root.controller.directoryMode ? "folder" : "document-open")
                    rendering: HnIcon.Original
                }
                trailingContent: Controls.ToolButton {
                    objectName: "close"
                    icon.name: "window-close"
                    Accessible.name: qsTr("Close")
                    onClicked: root.controller.cancel()
                }
            }
        }
        HnSeparator {
            Layout.fillWidth: true
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0
            Rectangle {
                id: placesPanel
                color: HoloniightPalette.surface
                objectName: "placesPanel"
                Layout.preferredWidth: root.sidebarWidth
                Layout.minimumWidth: root.sidebarWidth
                Layout.maximumWidth: root.sidebarWidth
                Layout.fillHeight: true
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: root.panelInset
                    spacing: root.panelInset
                    HnLabel {
                        role: HnTypographyRole.Caption
                        color: HoloniightPalette.textMuted
                        Layout.leftMargin: HnMetrics.horizontalPadding(HnControlSize.Compact)
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
                        delegate: HnListDelegate {
                            id: place
                            required property var modelData
                            width: places.width
                            sizeRole: HnControlSize.Xs
                            focusPolicy: Qt.NoFocus
                            leadingContentAlignment: Qt.AlignVCenter
                            title: modelData.name
                            currentItemSelects: false
                            highlighted: modelData.path === root.controller.location
                            leadingContent: Component {
                                HnIcon {
                                    size: HnMetrics.iconSize(HnControlSize.Compact)
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
                HnSeparator {
                    orientation: Qt.Vertical
                    crossAxisAlignment: HnSeparator.Trailing
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.right: parent.right
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: 0
                spacing: 0
                Rectangle {
                    objectName: "toolbarSurface"
                    color: HoloniightPalette.surface
                    Layout.fillWidth: true
                    implicitHeight: toolbar.implicitHeight + 2 * root.panelInset
                    GridLayout {
                        id: toolbar
                        objectName: "browserToolbar"
                        anchors.fill: parent
                        anchors.margins: root.panelInset
                        Layout.fillWidth: true
                        columns: root.wide ? 3 : 2
                        RowLayout {
                            spacing: 0
                            HnIconButton {
                                objectName: "historyBack"
                                sizeRole: HnControlSize.Compact
                                focusPolicy: Qt.NoFocus
                                icon.name: "go-previous"
                                Accessible.name: qsTr("Back")
                                enabled: root.controller.canGoBack && !root.controller.overwritePending
                                onClicked: root.controller.goBack()
                            }
                            HnIconButton {
                                objectName: "historyForward"
                                sizeRole: HnControlSize.Compact
                                focusPolicy: Qt.NoFocus
                                icon.name: "go-next"
                                Accessible.name: qsTr("Forward")
                                enabled: root.controller.canGoForward && !root.controller.overwritePending
                                onClicked: root.controller.goForward()
                            }
                            HnIconButton {
                                objectName: "parentFolder"
                                sizeRole: HnControlSize.Compact
                                focusPolicy: Qt.NoFocus
                                icon.name: "go-up"
                                Accessible.name: qsTr("Up")
                                onClicked: root.controller.parentFolder()
                            }
                        }
                        Item {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            implicitHeight: Math.max(breadcrumbs.implicitHeight, location.implicitHeight)
                            HnBreadcrumbBar {
                                id: breadcrumbs
                                anchors.fill: parent
                                Layout.fillWidth: true
                                visible: !root.editingLocation
                                segments: root.controller.breadcrumbLabels
                                onSegmentActivated: index => root.controller.activateBreadcrumb(index)
                                onLocationEditRequested: root.editLocation()
                            }
                            Controls.TextField {
                                id: location
                                anchors.fill: parent
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
                        }
                        HnSearchField {
                            id: query
                            objectName: "search"
                            Layout.columnSpan: root.wide ? 1 : 2
                            Layout.fillWidth: true
                            Layout.preferredWidth: 220
                            Layout.maximumWidth: root.wide ? 260 : Infinity
                            placeholderText: qsTr("Search this folder")
                            Accessible.name: placeholderText
                            onTextChanged: root.controller.search(text)
                            onAccepted: listing.forceActiveFocus()
                        }
                    }
                }
                HnSeparator {
                    Layout.fillWidth: true
                }
                RowLayout {
                    id: browserBody
                    objectName: "browserBody"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 0
                    spacing: 0
                    ColumnLayout {
                        id: browser
                        objectName: "browser"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.minimumWidth: 0
                        spacing: 0
                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.minimumHeight: 0
                            Layout.margins: 12
                            spacing: 0
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
                        ColumnLayout {
                            objectName: "browserForm"
                            Layout.fillWidth: true
                            Layout.margins: visible ? 12 : 0
                            visible: root.controller.save || (!root.wide && root.controller.filterNames.length > 0)
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

                            Item {
                                id: browserFilter
                                Layout.fillWidth: true
                                visible: root.controller.save || !root.wide
                                implicitHeight: typeFilter.implicitHeight
                            }
                        }
                    }
                    HnSeparator {
                        visible: root.wide
                        orientation: Qt.Vertical
                        Layout.fillHeight: true
                    }
                    Rectangle {
                        id: previewDock
                        objectName: "previewDock"
                        color: HoloniightPalette.surface
                        visible: root.wide
                        Layout.preferredWidth: root.sidebarWidth
                        Layout.minimumWidth: root.sidebarWidth
                        Layout.maximumWidth: root.sidebarWidth
                        Layout.fillHeight: true
                    }
                }
            }
        }
        HnSeparator {
            Layout.fillWidth: true
        }
        Rectangle {
            objectName: "footerSurface"
            color: HoloniightPalette.surface
            Layout.fillWidth: true
            implicitHeight: footerContent.implicitHeight + 2 * root.panelInset
            ColumnLayout {
                id: footerContent
                anchors.fill: parent
                anchors.margins: root.panelInset
                spacing: 8
                Controls.ScrollView {
                    objectName: "validation"
                    visible: root.controller.explanation.length > 0
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.min(64, explanation.implicitHeight)
                    contentWidth: availableWidth
                    Controls.Label {
                        id: explanation
                        width: parent.width
                        text: root.controller.explanation
                        wrapMode: Text.Wrap
                    }
                }
                GridLayout {
                    columns: root.wide ? 8 : 5
                    Layout.fillWidth: true
                    Controls.ToolButton {
                        objectName: "previewToggle"
                        Layout.row: 0
                        Layout.column: 0
                        visible: !root.wide
                        text: qsTr("Show preview")
                        onClicked: previewDrawer.open()
                    }
                    Controls.ToolButton {
                        objectName: "optionsToggle"
                        Layout.row: 0
                        Layout.column: 1
                        visible: root.controller.choices.length > 0
                        text: qsTr("Additional options")
                        onClicked: optionsPopup.open()
                    }
                    HnLabel {
                        Layout.row: root.wide ? 0 : 1
                        Layout.column: root.wide ? 2 : 0
                        role: HnTypographyRole.Caption
                        visible: root.controller.multiple
                        rawText: qsTr("%n selected", "", root.controller.selectedIds.length)
                    }
                    Controls.BusyIndicator {
                        Layout.row: root.wide ? 0 : 1
                        Layout.column: root.wide ? 3 : 1
                        Layout.preferredWidth: 24
                        Layout.preferredHeight: 24
                        running: root.controller.busy
                    }
                    Item {
                        Layout.row: root.wide ? 0 : 1
                        Layout.column: root.wide ? 4 : 2
                        Layout.fillWidth: true
                    }
                    Item {
                        id: footerFilter
                        Layout.row: 0
                        Layout.column: 5
                        Layout.preferredWidth: 200
                        visible: root.wide && !root.controller.save && root.controller.filterNames.length > 0
                        implicitHeight: typeFilter.implicitHeight
                    }
                    Controls.Button {
                        objectName: "cancel"
                        Layout.row: root.wide ? 0 : 1
                        Layout.column: root.wide ? 6 : 3
                        text: qsTr("Cancel")
                        onClicked: root.controller.cancel()
                    }
                    Controls.Button {
                        objectName: "accept"
                        Layout.row: root.wide ? 0 : 1
                        Layout.column: root.wide ? 7 : 4
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
    }
    Controls.ComboBox {
        id: typeFilter
        objectName: "filter"
        parent: root.controller.save || !root.wide ? browserFilter : footerFilter
        anchors.fill: parent
        visible: root.controller.filterNames.length > 0
        model: root.controller.filterNames
        currentIndex: root.controller.filterIndex
        Accessible.name: qsTr("File type")
        onActivated: index => root.controller.filterIndex = index
    }
    Controls.Drawer {
        id: previewDrawer
        objectName: "previewDrawer"
        edge: Qt.RightEdge
        y: browserBody.mapToItem(root.contentItem, 0, 0).y
        width: root.sidebarWidth
        height: browserBody.height
        background: Rectangle {
            color: HoloniightPalette.surface
            HnSeparator {
                orientation: Qt.Vertical
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.left: parent.left
            }
        }
        modal: false
        focus: true
        closePolicy: Controls.Popup.CloseOnEscape | Controls.Popup.CloseOnPressOutside
        contentItem: ColumnLayout {
            Controls.ToolButton {
                Layout.alignment: Qt.AlignRight
                text: qsTr("Close preview")
                onClicked: previewDrawer.close()
            }
            Item {
                id: drawerPreview
                Layout.fillWidth: true
                Layout.fillHeight: true
            }
            Shortcut {
                sequence: "Escape"
                enabled: previewDrawer.visible && !root.controller.overwritePending
                onActivated: previewDrawer.close()
            }
        }
    }
    FilePreview {
        objectName: "entryPreview"
        parent: root.wide ? previewDock : drawerPreview
        anchors.fill: parent
        anchors.margins: 12
        visible: root.wide || previewDrawer.visible
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

    Controls.Popup {
        id: optionsPopup
        objectName: "optionsPopup"
        anchors.centerIn: parent
        width: Math.min(480, root.width - 32)
        height: Math.min(320, root.height - 48)
        modal: true
        focus: true
        closePolicy: Controls.Popup.CloseOnEscape | Controls.Popup.CloseOnPressOutside
        Controls.ScrollView {
            id: optionsScroll
            objectName: "optionsScroll"
            anchors.fill: parent
            contentWidth: availableWidth
            ColumnLayout {
                width: optionsScroll.availableWidth
                Repeater {
                    model: root.controller.choices
                    ColumnLayout {
                        id: choice
                        required property int index
                        required property var modelData
                        Layout.fillWidth: true
                        Controls.Label {
                            text: choice.modelData.label
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                        }
                        Controls.CheckBox {
                            objectName: "choice-" + choice.index
                            visible: choice.modelData.items.length === 0
                            checked: choice.modelData.selected === "true"
                            Accessible.name: choice.modelData.label
                            onClicked: root.controller.setChoice(choice.index, checked ? "true" : "false")
                        }
                        Controls.ComboBox {
                            objectName: "choice-" + choice.index
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
        enabled: !root.controller.overwritePending && !previewDrawer.visible && !optionsPopup.visible
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
