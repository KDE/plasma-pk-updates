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
import org.kde.plasma.components 2.0
import org.kde.plasma.PackageKit 1.0

Item
{
    Binding {
        target: timestampLabel
        property: "text"
        value: PkUpdates.timestamp
        when: !plasmoid.expanded
    }

    PlasmaExtras.Heading {
        id: header
        Layout.fillWidth: true
        level: 3
        wrapMode: Text.WordWrap
        text: PkUpdates.message
    }

    ColumnLayout
    {
        anchors {
            fill: parent
            topMargin: header.height
        }
        Label {
            visible: PkUpdates.isActive
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: i18n("Status: %1", PkUpdates.statusMessage)
        }
        ProgressBar {
            visible: PkUpdates.isActive
            Layout.fillWidth: true
            minimumValue: 0
            maximumValue: 101 // workaround a bug in ProgressBar! if the value is > max, it's set to max and never changes below
            value: PkUpdates.percentage
            indeterminate: PkUpdates.percentage > 100
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            visible: PkUpdates.count
            text: PkUpdates.packageNames
        }
        Button {
            visible: !PkUpdates.isActive
            anchors.horizontalCenter: parent.horizontalCenter
            text: PkUpdates.isSystemUpToDate ? i18n("Check For Updates") : i18n("Review Updates")
            tooltip: PkUpdates.isSystemUpToDate ? i18n("Checks for any available updates") : i18n("Launches the software to perform the update")
            onClicked: PkUpdates.isSystemUpToDate ? PkUpdates.checkUpdates(true) : PkUpdates.reviewUpdates()
        }
        Label {
            visible: !PkUpdates.isActive
            id: timestampLabel
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: PkUpdates.timestamp
        }
    }
}
