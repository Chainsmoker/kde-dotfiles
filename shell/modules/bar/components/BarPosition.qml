import QtQuick
import Quickshell
import Caelestia.Config
import qs.components
import qs.services
import qs.utils

Item {
    id: root

    // Clockwise order: top, right, bottom, left. A left-click advances it, a
    // right-click reverses it. The button shows where the bar currently sits.
    readonly property var positionOrder: ["top", "right", "bottom", "left"]
    readonly property string currentPosition: Config.bar.position

    function iconFor(position: string): string {
        if (position === "top")
            return "arrow_upward";
        if (position === "bottom")
            return "arrow_downward";
        if (position === "left")
            return "arrow_back";
        return "arrow_forward";
    }

    function cycle(step: int): void {
        const index = positionOrder.indexOf(currentPosition);
        const next = positionOrder[(index + step + positionOrder.length) % positionOrder.length];

        GlobalConfig.bar.position = next;
        // Screens with a per-monitor position override would otherwise ignore the
        // global change, the same reason the Nexus page resets them.
        for (let i = 0; i < Quickshell.screens.length; i++)
            GlobalConfig.forScreen(Quickshell.screens[i].name).bar.resetOption("position");
    }

    implicitWidth: icon.implicitHeight + Tokens.padding.small
    implicitHeight: icon.implicitHeight

    StateLayer {
        // Cursed workaround to make the height larger than the parent
        anchors.fill: undefined
        anchors.centerIn: parent
        implicitWidth: implicitHeight
        implicitHeight: icon.implicitHeight + Tokens.padding.small
        radius: Tokens.rounding.full
        Accessible.name: qsTr("Bar position")
        Accessible.role: Accessible.Button
        Accessible.description: qsTr("Left-click to move the bar clockwise: top, right, bottom, left. Right-click to move it back. The arrow points at the current screen edge.")
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onClicked: mouse => root.cycle(mouse.button === Qt.RightButton ? -1 : 1)
    }

    MaterialIcon {
        id: icon

        anchors.centerIn: parent
        anchors.horizontalCenterOffset: Centering.pixelAlign(parent.width, width)

        text: root.iconFor(root.currentPosition)
        color: Colours.palette.m3onSurfaceVariant
        fontStyle: Tokens.font.icon.builders.small.weight(Font.Bold).build()
    }
}
