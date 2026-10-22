pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts

Rectangle {
    id: root
    required property var controller
    color: systemPalette.window
    SystemPalette {
        id: systemPalette
    }
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12
        GridLayout {
            columns: 3
            Layout.fillWidth: true
            Repeater {
                model: ["Open file", "Open multiple files", "Select folder", "Select multiple folders", "Save file", "Save multiple files"]
                Controls.Button {
                    required property int index
                    required property string modelData
                    objectName: "picker" + index
                    text: modelData
                    enabled: root.controller.enabled
                    Layout.fillWidth: true
                    onClicked: root.controller.request(index)
                }
            }
        }
        Controls.ScrollView {
            id: scroll
            objectName: "activityLog"
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            Flickable {
                id: viewport
                objectName: "logViewport"
                clip: true
                contentWidth: width
                contentHeight: log.implicitHeight
                Controls.TextArea {
                    id: log
                    width: viewport.width
                    readOnly: true
                    selectByMouse: true
                    textFormat: TextEdit.PlainText
                    wrapMode: TextEdit.Wrap
                    text: root.controller.log
                    property bool following: true
                    onTextChanged: {
                        if (following)
                            Qt.callLater(function () {
                                viewport.contentY = Math.max(0, viewport.contentHeight - viewport.height);
                            });
                    }
                }
                onContentYChanged: log.following = contentY >= contentHeight - height - 2
            }
        }
    }
}
