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

import QtQuick 2.1
import QtQuick.Layouts 1.1
import QtQuick.Controls 1.3
import org.kde.plasma.components 2.0 as PlasmaComponents
import org.kde.plasma.extras 2.0 as PlasmaExtras
import org.kde.plasma.core 2.0 as PlasmaCore

PlasmaComponents.ListItem {
    id: packageDelegate

    property string updateText
    property variant updateUrls: []
    readonly property bool expanded: ListView.isCurrentItem

    signal checkedStateChanged(bool checked)

    height: packageInfoColumn.height + detailsInfoColumn.height + Math.round(units.gridUnit / 2)
    width: parent.width
    enabled: true
    checked: containsMouse || expanded

    PlasmaComponents.CheckBox {
        id: checkbox
        anchors {
            left: parent.left
            verticalCenter: packageInfoColumn.verticalCenter
        }
        checked: selected
        onClicked: {
            updatesModel.setProperty(index, "selected", checked)
            packageDelegate.checkedStateChanged(checked)
        }
    }

    Column {
        id: packageInfoColumn

        height: packageNameLabel.height + packageDescriptionLabel.height
        anchors {
            left: checkbox.right
            right: parent.right
            top: parent.top
            leftMargin: Math.round(units.gridUnit / 2)
            rightMargin: Math.round(units.gridUnit / 2)
        }

        PlasmaComponents.Label {
            id: packageNameLabel
            height: paintedHeight
            anchors {
                left: parent.left
                right: parent.right
            }
            elide: Text.ElideRight;
            text: i18nc("Package Name (Version)", "%1 (%2)", name, version)
        }

        PlasmaComponents.Label {
            id: packageDescriptionLabel
            height: paintedHeight
            anchors {
                left: parent.left
                right: parent.right
            }
            elide: Text.ElideRight;
            font.pointSize: theme.smallestFont.pointSize;
            opacity: 0.6;
            text: desc
        }
    }

    Column {
        id: detailsInfoColumn
        height: packageDelegate.expanded ? childrenRect.height + Math.round(units.gridUnit / 2) : 0
        visible: expanded
        spacing: 2

        anchors {
            left: checkbox.right
            right: parent.right
            top: packageInfoColumn.bottom
            leftMargin: Math.round(units.gridUnit / 2)
            rightMargin: Math.round(units.gridUnit / 2)
            topMargin: Math.round(units.gridUnit / 3)
        }

        PlasmaCore.SvgItem {
            id: detailsSeparator;
            height: lineSvg.elementSize("horizontal-line").height;
            width: parent.width;
            anchors {
                left: parent.left;
                right: parent.right;
            }
            elementId: "horizontal-line";

            svg: PlasmaCore.Svg {
                id: lineSvg;
                imagePath: "widgets/line";
            }
        }

        PlasmaComponents.Label {
            id: descriptionLabel
            height: paintedHeight
            anchors {
                left: parent.left
                right: parent.right
            }
            font.weight: Font.DemiBold
            text: i18nc("description of the update", "Update Description")
        }

        PlasmaComponents.Label {
            id: descriptionContentLabel
            height: paintedHeight
            anchors {
                left: parent.left
                right: parent.right
            }
            font.pointSize: theme.smallestFont.pointSize;
            opacity: 0.6;
            text: updateText == "" ? i18n("No description available") : updateText
            wrapMode: Text.WordWrap
        }

        PlasmaComponents.Label {
            id: urlsLabel
            height: visible ? paintedHeight : 0
            visible: !!updateUrls
            anchors {
                left: parent.left
                right: parent.right
            }
            font.weight: Font.DemiBold
            text: i18n("Related URLs")
        }

        Repeater {
            model: updateUrls
            PlasmaComponents.Label {
                height: paintedHeight
                color: theme.linkColor
                font.pointSize: theme.smallestFont.pointSize
                font.underline: true
                opacity: 0.6;
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
