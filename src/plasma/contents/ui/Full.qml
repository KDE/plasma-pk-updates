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
import org.kde.plasma.extras 2.0 as PlasmaExtras
import org.kde.plasma.core 2.0 as PlasmaCore
//import org.kde.plasma.components 2.0 as PC
import org.kde.plasma.PackageKit 1.0

Item
{
    Binding {
        target: timestampLabel
        property: "text"
        value: PkUpdates.timestamp
        when: !plasmoid.expanded
    }

    Component.onCompleted: {
        PkUpdates.updatesChanged.connect(populateModel)
        populateModel()
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
        nameColumn.resizeToContents()
    }

    ListModel {
        id: updatesModel
    }

    PlasmaExtras.Heading {
        id: header
        level: 3
        wrapMode: Text.WordWrap
        text: PkUpdates.message
    }

    ColumnLayout
    {
        spacing: units.largeSpacing
        anchors {
            fill: parent
            topMargin: header.height
        }

        CheckBox {
            id: dummyCheckBox
            visible: false
        }

        TableView {
            id: updatesView
            model: PlasmaCore.SortFilterModel {
                sourceModel: updatesModel
                filterRole: "name"
            }
            sortIndicatorColumn: 1
            //sortIndicatorVisible: true
            focus: visible
            headerVisible: false // BUG looks broken with default Qt style
            visible: PkUpdates.count && !PkUpdates.isActive
            Layout.fillWidth: true
            Layout.fillHeight: true

            TableViewColumn {
                id: checkColumn
                role: "selected"
                width: dummyCheckBox.width
                resizable: false
                movable: false
                delegate: CheckBox {
                    id: checkbox
                    checked: styleData.value
                    onClicked: {
                        updatesModel.setProperty(styleData.row, "selected", checked)
                    }
                }
            }
            TableViewColumn { id: nameColumn; role: "name"; title: i18n("Package"); width: 200 }
            TableViewColumn { role: "desc"; title: i18n("Description"); width: 600 }
        }

        Button {
            visible: !PkUpdates.isActive && PkUpdates.isNetworkOnline
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: statusbar.top
            anchors.bottomMargin: units.largeSpacing
            text: PkUpdates.isSystemUpToDate ? i18n("Check For Updates") : i18n("Install Updates")
            tooltip: PkUpdates.isSystemUpToDate ? i18n("Checks for any available updates") : i18n("Performs the software update")
            onClicked: PkUpdates.isSystemUpToDate ? PkUpdates.checkUpdates(needsForcedUpdate()) : PkUpdates.installUpdates(selectedPackages())
        }

        BusyIndicator {
            running: PkUpdates.isActive
            visible: running
            anchors.horizontalCenter: parent.horizontalCenter
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
