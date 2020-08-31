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
import QtQuick.Controls 2.5 as QQC2
import QtQuick.Dialogs 1.2
import org.kde.plasma.components 3.0 as PlasmaComponents3
import org.kde.plasma.extras 2.0 as PlasmaExtras
import org.kde.plasma.core 2.0 as PlasmaCore
import org.kde.plasma.PackageKit 1.0

Item {
    id: fullRepresentation

    property bool anySelected: false
    property bool allSelected: false
    property bool populatePreSelected: true

    width: units.gridUnit * 20
    height: units.gridUnit * 20

    Binding {
        target: timestampLabel
        property: "text"
        value: PkUpdates.timestamp
        when: !plasmoid.expanded
    }

    Connections {
        target: PkUpdates
        onUpdatesChanged: populateModel()
        onUpdateDetail: updateDetails(packageID, updateText, urls)
        onUpdatesInstalled: plasmoid.expanded = false
        onEulaRequired: eulaDialog.showPrompt(eulaID, packageID, vendor, licenseAgreement)
    }

    Component.onCompleted: populateModel()

    Dialog {
        property string eulaID: ""
        property string packageName: ""
        property string vendor: ""
        property string licenseText: ""

        property bool buttonClicked: false

        id: eulaDialog
        title: i18n("License Agreement for %1").arg(packageName)
        standardButtons: StandardButton.Yes | StandardButton.No

        ColumnLayout {
            anchors.fill: parent

            QQC2.Label {
                text: i18n("License agreement required for %1 (from %2):").arg(eulaDialog.packageName).arg(eulaDialog.vendor)
            }

            QQC2.TextArea {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: 400
                Layout.minimumHeight: 200
                text: eulaDialog.licenseText
                readOnly: true
            }

            QQC2.Label {
                text: i18n("Do you accept?")
            }
        }

        onVisibleChanged: {
            // onRejected does not fire on dialog closing, so implement that ourselves
            if(!visible && !buttonClicked)
                onNo();
        }
        onNo: {
            buttonClicked = true;
            PkUpdates.eulaAgreementResult(this.eulaID, false);
        }
        onYes: {
            buttonClicked = true;
            PkUpdates.eulaAgreementResult(this.eulaID, true);
        }

        function showPrompt(eulaID, packageID, vendor, licenseAgreement) {
            this.eulaID = eulaID;
            this.packageName = PkUpdates.packageName(packageID);
            this.vendor = vendor;
            this.licenseText = licenseAgreement;

            this.visible = true;
        }
    }

    ListModel {
        id: updatesModel
    }


    ColumnLayout {
        id: statusbar

        anchors.fill: parent

        spacing: units.smallSpacing

        PlasmaExtras.Heading {
            Layout.fillWidth: true
            level: 4
            wrapMode: Text.WordWrap
            text: !PkUpdates.isNetworkOnline ? i18n("Network is offline") : PkUpdates.message
        }

        PlasmaComponents3.Label {
            visible: PkUpdates.isActive || PkUpdates.count === 0
            font.pointSize: theme.smallestFont.pointSize;
            opacity: 0.6;
            text: {
                if (PkUpdates.isActive)
                    return PkUpdates.statusMessage
                else if (PkUpdates.isNetworkOnline)
                    return i18n("Updates are automatically checked %1.",
                                updateInterval(plasmoid.configuration.daily,
                                               plasmoid.configuration.weekly,
                                               plasmoid.configuration.monthly));
                return ""
            }
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
        }

        PlasmaComponents3.Label {
            id: timestampLabel
            Layout.fillWidth: true
            visible: !PkUpdates.isActive
            wrapMode: Text.WordWrap
            font.italic: true
            font.pointSize: theme.smallestFont.pointSize;
            opacity: 0.6;
            text: PkUpdates.timestamp
        }

        PlasmaComponents3.ProgressBar {
            Layout.fillWidth: true
            visible: PkUpdates.isActive
            from: 0
            to: 101 // BUG workaround a bug in ProgressBar! if the value is > max, it's set to max and never changes below
            value: PkUpdates.percentage
            indeterminate: PkUpdates.percentage > 100
        }

        PlasmaExtras.ScrollArea {
            id: listViewScrollArea

            Layout.fillWidth: true
            Layout.fillHeight: true

            visible: PkUpdates.count && PkUpdates.isNetworkOnline && !PkUpdates.isActive

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
                    onCheckStateChanged: updateSelectionState();
                }
            }
        }

        // Container for other items that can be shown when the main scroll
        // view is not visible
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            visible: !listViewScrollArea.visible

            PlasmaComponents3.BusyIndicator {
                running: PkUpdates.isActive && PkUpdates.count == 0
                visible: running
                anchors.centerIn: parent
            }

            PlasmaExtras.PlaceholderMessage {
                anchors.centerIn: parent
                width: parent.width - (units.largeSpacing * 4)

                visible: PkUpdates.count === 0 && PkUpdates.isNetworkOnline && !PkUpdates.isActive

                text: i18n("No updates available")

                helpfulAction: QQC2.Action {
                    icon.name: "view-refresh"
                    text: "Check for Updates"
                    onTriggered: {
                        PkUpdates.checkUpdates(true /* manual */) // circumvent the checks, the user knows what they're doing ;)
                    }
                }
            }
        }

        PlasmaComponents3.CheckBox {
            Layout.fillWidth: true
            Layout.leftMargin: units.smallSpacing

            visible: PkUpdates.count !== 0 && PkUpdates.isNetworkOnline && !PkUpdates.isActive

            tristate: true

            checkState: fullRepresentation.allSelected ? Qt.Checked :
                        (fullRepresentation.anySelected ? Qt.PartiallyChecked
                                                        : Qt.Unchecked)

            text: i18n("Select all packages")

            onClicked: {
                populatePreSelected = !fullRepresentation.anySelected;
                populateModel();
            }
        }

        PlasmaComponents3.Button {
            visible: PkUpdates.count !== 0 && PkUpdates.isNetworkOnline && !PkUpdates.isActive
            icon.name: "install"
            enabled: fullRepresentation.anySelected
            Layout.alignment: Qt.AlignHCenter
            text: i18n("Install Updates")
            onClicked: PkUpdates.installUpdates(selectedPackages())

            PlasmaComponents3.ToolTip {
                text: i18n("Performs the software update")
            }
        }
    }

    function updateSelectionState() {
        console.log("Updating state of selection");
        var anySelected = false;
        var allSelected = true;
        for (var i = 0; i < updatesModel.count; i++) {
            var pkg = updatesModel.get(i)
            if (pkg.selected)
                anySelected = true;
            else
                allSelected = false;

            if (anySelected && !allSelected)
                break; // Can't change anymore
        }
        fullRepresentation.anySelected = anySelected;
        fullRepresentation.allSelected = allSelected;
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
        updateSelectionState();
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
