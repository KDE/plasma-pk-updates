/***************************************************************************
 *   Copyright (C) 2013 by Aleix Pol Gonzalez <aleixpol@blue-systems.com>  *
 *   Copyright (C) 2015 by Lukáš Tinkl <lukas@kde.org>                     *
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
import org.kde.plasma.PackageKit 1.0

Item {
    readonly property bool __anySelected: {
        for (var i = 0; i < updatesModel.count; i++) {
            var pkg = updatesModel.get(i)
            if (pkg.selected)
                return true
        }
        return false
    }

    Binding {
        target: timestampLabel
        property: "text"
        value: PkUpdates.timestamp
        when: !plasmoid.expanded
    }

    Component.onCompleted: {
        PkUpdates.updatesChanged.connect(populateModel)
    }

    function populateModel() {
        print("Populating model")
        updatesModel.clear()
        var packages = PkUpdates.packages
        for (var id in packages) {
            if (packages.hasOwnProperty(id)) {
                var desc = packages[id]
                updatesModel.append({"selected": true, "id": id, "name": PkUpdates.packageName(id), "desc": desc})
            }
        }
    }

    ListModel {
        id: updatesModel
    }

    PlasmaExtras.Heading {
        id: header
        level: 3
        wrapMode: Text.WordWrap
        text: !PkUpdates.isNetworkOnline ? i18n("Network is offline") : PkUpdates.message
    }

    ColumnLayout {
        spacing: units.largeSpacing
        anchors {
            fill: parent
            topMargin: header.height
        }

        PlasmaExtras.ScrollArea {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: PkUpdates.count && !PkUpdates.isActive

            ListView {
                id: updatesView
                clip: true
                model: PlasmaCore.SortFilterModel {
                    sourceModel: updatesModel
                    filterRole: "name"
                }
                anchors.fill: parent
                currentIndex: -1
                boundsBehavior: Flickable.StopAtBounds
                delegate: packageComponent

                Component {
                    id: packageComponent

                    PlasmaComponents.ListItem {
                        height: packageInfoColumn.height + units.gridUnit
                        width: parent.width

                        PlasmaComponents.CheckBox {
                            id: checkbox
                            anchors {
                                left: parent.left
                                verticalCenter: packageInfoColumn.verticalCenter
                            }
                            checked: selected
                            onClicked: {
                                updatesModel.setProperty(index, "selected", checked)
                            }
                        }

                        Column {
                            id: packageInfoColumn

                            anchors {
                                left: checkbox.right
                                right: parent.right
                                verticalCenter: parent.verticalCenter
                                leftMargin: Math.round(units.gridUnit / 2)
                                rightMargin: Math.round(units.gridUnit / 2)
                            }
                            height: packageNameLabel.height + packageDescriptionLabel.height

                            PlasmaComponents.Label {
                                id: packageNameLabel
                                height: paintedHeight
                                anchors {
                                    left: parent.left
                                    right: parent.right
                                }
                                elide: Text.ElideRight;
                                text: name
                            }

                            PlasmaComponents.Label {
                                id: packageDescriptionLabel
                                height: paintedHeight
                                anchors {
                                    left: parent.left
                                    right: parent.right
                                }
                                elide: Text.ElideRight;
                                font.italic: true
                                font.pointSize: theme.smallestFont.pointSize;
                                opacity: 0.6;
                                text: desc
                            }
                        }
                    }
                }
            }
        }

        Button {
            id: btnCheck
            visible: !PkUpdates.count && PkUpdates.isNetworkOnline
            enabled: !PkUpdates.isActive
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: statusbar.top
            anchors.bottomMargin: units.largeSpacing
            text: i18n("Check For Updates")
            tooltip: i18n("Checks for any available updates")
            onClicked: PkUpdates.checkUpdates(needsForcedUpdate()) // circumvent the checks, the user knows what they're doing ;)
        }

        Button {
            id: btnUpdate
            visible: PkUpdates.count && PkUpdates.isNetworkOnline
            enabled: __anySelected && !PkUpdates.isActive
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: statusbar.top
            anchors.bottomMargin: units.largeSpacing
            text: i18n("Install Updates")
            tooltip: i18n("Performs the software update")
            onClicked: PkUpdates.installUpdates(selectedPackages())
        }

        BusyIndicator {
            running: PkUpdates.isActive
            visible: running
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
        }

        RowLayout {
            id: statusbar
            spacing: units.largeSpacing
            anchors.bottom: parent.bottom
            Label {
                visible: !PkUpdates.isActive
                id: timestampLabel
                wrapMode: Text.WordWrap
                text: PkUpdates.timestamp
            }
            Label {
                visible: PkUpdates.isActive
                wrapMode: Text.WordWrap
                text: PkUpdates.statusMessage
            }
            ProgressBar {
                visible: PkUpdates.isActive
                Layout.fillWidth: true
                minimumValue: 0
                maximumValue: 101 // BUG workaround a bug in ProgressBar! if the value is > max, it's set to max and never changes below
                value: PkUpdates.percentage
                indeterminate: PkUpdates.percentage > 100
            }
        }
    }

    function selectedPackages() {
        var result = []
        for (var i = 0; i < updatesModel.count; i++) {
            var pkg = updatesModel.get(i)
            if (pkg.selected) {
                print("Package " + pkg.id + " selected for update")
                result.push(pkg.id)
            }
        }
        return result
    }
}
