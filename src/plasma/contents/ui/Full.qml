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
import org.kde.plasma.PackageKit 1.0

Item {
    id: fullRepresentation

    property bool anySelected: checkAnySelected()
    property bool allSelected: checkAllSelected()
    property bool populatePreSelected: true

    Binding {
        target: timestampLabel
        property: "text"
        value: PkUpdates.timestamp
        when: !plasmoid.expanded
    }

    Connections {
        target: PkUpdates
        onUpdatesChanged: populateModel()
        onUpdateDetail: updateDetails()
    }

    Component.onCompleted: populateModel()

    ListModel {
        id: updatesModel
    }

    PlasmaExtras.Heading {
        id: header
        level: 4
        wrapMode: Text.WordWrap
        text: !PkUpdates.isNetworkOnline ? i18n("Network is offline") : PkUpdates.message
    }

    ColumnLayout {
        id: statusbar
        anchors {
            left: parent.left
            right: parent.right
            top: header.bottom
            rightMargin: Math.round(units.gridUnit / 2)
        }
        spacing: units.largeSpacing

        PlasmaComponents.Label {
            id: timestampLabel
            visible: !PkUpdates.isActive
            wrapMode: Text.WordWrap
            font.italic: true
            font.pointSize: theme.smallestFont.pointSize;
            opacity: 0.6;
            text: PkUpdates.timestamp
        }

        PlasmaComponents.Label {
            visible: PkUpdates.isActive || !PkUpdates.count
            font.pointSize: theme.smallestFont.pointSize;
            opacity: 0.6;
            text: {
                if (PkUpdates.isActive)
                    return PkUpdates.statusMessage
                else if (PkUpdates.isNetworkOnline)
                    return i18n("Updates are automatically checked %1.<br>" +
                                "Click the 'Check For Updates' button below to search for updates manually.",
                                updateInterval(plasmoid.configuration.daily,
                                               plasmoid.configuration.weekly,
                                               plasmoid.configuration.monthly));
                return ""
            }
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
        }
    }

    ProgressBar {
        id: progressBar
        anchors {
            left: parent.left
            right: parent.right
            top: statusbar.bottom
        }
        visible: PkUpdates.isActive
        minimumValue: 0
        maximumValue: 101 // BUG workaround a bug in ProgressBar! if the value is > max, it's set to max and never changes below
        value: PkUpdates.percentage
        indeterminate: PkUpdates.percentage > 100
    }

    ColumnLayout {
        spacing: units.smallSpacing
        anchors {
            bottom: parent.bottom
            left: parent.left
            right: parent.right
            top: statusbar.bottom
        }

        PlasmaExtras.ScrollArea {
            id: updatesScrollArea
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
                delegate: PackageDelegate {
                    onClicked: {
                        if (updatesView.currentIndex === index) {
                            updatesView.currentIndex = -1
                        } else {
                            updatesView.currentIndex = index
                            PkUpdates.getUpdateDetails(id)
                        }
                    }
                    onCheckedStateChanged: {
                        if (checked) {
                            fullRepresentation.anySelected = true
                        } else {
                            fullRepresentation.anySelected = checkAnySelected()
                        }
                    }
                }
            }
        }

        RowLayout {
            visible: PkUpdates.count && !PkUpdates.isActive
            PlasmaComponents.CheckBox {
                id: chkSelectAll
                anchors {
                    left: parent.left
                    leftMargin: Math.round(units.gridUnit / 3)
                }
                checkedState: fullRepresentation.anySelected ? (fullRepresentation.allSelected ? Qt.Checked : Qt.PartiallyChecked) : Qt.Unchecked
                partiallyCheckedEnabled: true
            }

            PlasmaComponents.Label {
                id: lblSelectAll
                height: paintedHeight
                anchors {
                    left: chkSelectAll.right
                    leftMargin: Math.round(units.gridUnit / 2)
                }
                elide: Text.ElideRight;
                text: i18n("Select all packages")
            }

            MouseArea {
                anchors.fill: parent
                enabled: true

                onClicked: {
                    if (chkSelectAll.checkedState == Qt.Unchecked) {
                        populatePreSelected = true
                        populateModel()
                    } else if (chkSelectAll.checkedState == Qt.PartiallyChecked) {
                        populatePreSelected = true
                        populateModel()
                    } else {
                        populatePreSelected = false
                        populateModel()
                    }
                    fullRepresentation.anySelected = checkAnySelected()
                }
            }
        }

        PlasmaComponents.Button {
            id: btnCheck
            visible: !PkUpdates.count && PkUpdates.isNetworkOnline && !PkUpdates.isActive
            enabled: !PkUpdates.isActive
            anchors {
                bottom: parent.bottom
                bottomMargin: Math.round(units.gridUnit / 3)
                horizontalCenter: parent.horizontalCenter
            }
            text: i18n("Check For Updates")
            tooltip: i18n("Checks for any available updates")
            onClicked: PkUpdates.checkUpdates() // circumvent the checks, the user knows what they're doing ;)
        }

        PlasmaComponents.Button {
            id: btnUpdate
            visible: PkUpdates.count && PkUpdates.isNetworkOnline && !PkUpdates.isActive
            enabled: fullRepresentation.anySelected
            anchors {
                bottom: parent.bottom
                bottomMargin: Math.round(units.gridUnit / 3)
                horizontalCenter: parent.horizontalCenter
            }
            text: i18n("Install Updates")
            tooltip: i18n("Performs the software update")
            onClicked: PkUpdates.installUpdates(selectedPackages())
        }

        PlasmaComponents.BusyIndicator {
            running: PkUpdates.isActive
            visible: running
            anchors.centerIn: parent
        }
    }

    function checkAnySelected() {
        for (var i = 0; i < updatesModel.count; i++) {
            var pkg = updatesModel.get(i)
            if (pkg.selected)
                return true
        }
        return false
    }

    function checkAllSelected() {
        for (var i = 0; i < updatesModel.count; i++) {
            var pkg = updatesModel.get(i)
            if (!pkg.selected)
                return false
        }
        return true
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

    function populateModel() {
        print("Populating model")
        updatesModel.clear()
        var packages = PkUpdates.packages
        for (var id in packages) {
            if (packages.hasOwnProperty(id)) {
                var desc = packages[id]
                updatesModel.append({"selected": populatePreSelected, "id": id, "name": PkUpdates.packageName(id), "desc": desc, "version": PkUpdates.packageVersion(id)})
            }
        }
    }

    function updateDetails(packageID, updateText, urls) {
        //print("Got update details for: " + packageID)
        print("Update text: " + updateText)
        print("URLs: " + urls)
        updatesView.currentItem.updateText = updateText
        updatesView.currentItem.updateUrls = urls
    }

    function updateInterval(daily, weekly, monthly) {
        if (weekly)
            return i18n("weekly");
        else if (monthly)
            return i18n("monthly");

        return i18n("daily");
    }
}
