/***************************************************************************
 *   Copyright (C) 2013 by Aleix Pol Gonzalez <aleixpol@blue-systems.com>  *
 *   Copyright (C) 2015 by Lukáš Tinkl <lukas@kde.org>                     *
 *   Copyright (C) 2015 by Jan Grulich <jgrulich@redhat.com>               *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA .        *
 ***************************************************************************/

import QtQuick 2.5
import QtQuick.Layouts 1.13
import org.kde.plasma.core 2.0 as PlasmaCore
import org.kde.plasma.components 3.0 as PlasmaComponents3
import org.kde.plasma.extras 2.0 as PlasmaExtras

PlasmaExtras.ListItem {
    id: packageDelegate

    property string updateText
    property variant updateUrls: []
    readonly property bool expanded: ListView.isCurrentItem

    signal checkStateChanged(bool checked)

    height: innerLayout.height + (units.smallSpacing * 2)
    enabled: true
    checked: containsMouse || expanded
    // The binding is overwritten on clicks, as this is for some reason a Button
    onClicked: checked = Qt.binding(function(){ return containsMouse || expanded; });

    RowLayout {
        id: innerLayout
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter

        PlasmaComponents3.CheckBox {
            Layout.alignment: Qt.AlignVCenter
            checked: selected
            onClicked: {
                updatesModel.setProperty(index, "selected", checked)
                packageDelegate.checkStateChanged(checked)
            }
        }

        ColumnLayout {
            spacing: units.smallSpacing / 2

            PlasmaComponents3.Label {
                Layout.fillWidth: true
                elide: Text.ElideRight
                text: i18nc("Package Name (Version)", "%1 (%2)", name, version)
            }

            PlasmaComponents3.Label {
                Layout.fillWidth: true
                elide: Text.ElideRight
                font.pointSize: theme.smallestFont.pointSize
                opacity: 0.6
                text: desc
            }

            ColumnLayout {
                visible: packageDelegate.expanded
                spacing: units.smallSpacing / 2

                PlasmaCore.SvgItem {
                    Layout.preferredHeight: lineSvg.elementSize("horizontal-line").height
                    Layout.fillWidth: true

                    elementId: "horizontal-line";

                    svg: PlasmaCore.Svg {
                        id: lineSvg;
                        imagePath: "widgets/line";
                    }
                }

                PlasmaComponents3.Label {
                    Layout.fillWidth: true
                    font.weight: Font.DemiBold
                    text: i18nc("description of the update", "Update Description")
                }

                PlasmaComponents3.Label {
                    Layout.fillWidth: true
                    font.pointSize: theme.smallestFont.pointSize
                    opacity: 0.6
                    text: updateText == "" ? i18n("No description available") : updateText
                    wrapMode: Text.WordWrap
                }

                PlasmaComponents3.Label {
                    visible: updateUrls.count > 0
                    Layout.fillWidth: true
                    font.weight: Font.DemiBold
                    text: i18n("Related URLs")
                }

                Repeater {
                    model: updateUrls
                    PlasmaComponents3.Label {
                        Layout.fillWidth: true
                        color: theme.linkColor
                        font.pointSize: theme.smallestFont.pointSize
                        font.underline: true
                        opacity: 0.6
                        text: modelData
                        wrapMode: Text.WordWrap

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor

                            onClicked: {
                                Qt.openUrlExternally(modelData)
                            }
                        }
                    }
                }
            }
        }
    }
}
