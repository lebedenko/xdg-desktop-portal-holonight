import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts

Rectangle {
    id: root

    required property var request
    required property string appName
    required property bool choice
    required property string description

    width: 420
    height: content.implicitHeight + 48
    color: body.palette.window
    focus: true

    Keys.onEscapePressed: root.request.reject()

    ColumnLayout {
        id: content
        anchors.margins: 24
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        spacing: 16

        Controls.Label {
            id: body
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            text: qsTr("Allow %1 to %2?").arg(root.appName || qsTr("this application")).arg(root.description)
        }

        RowLayout {
            Layout.alignment: Qt.AlignRight

            Controls.Button {
                text: qsTr("Cancel")
                onClicked: root.request.reject()
            }

            Controls.Button {
                visible: root.choice
                text: qsTr("Area")
                onClicked: root.request.approve(true)
            }

            Controls.Button {
                text: root.choice ? qsTr("Desktop") : qsTr("Allow")
                onClicked: root.request.approve(false)
            }
        }
    }
}
