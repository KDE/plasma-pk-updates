/***************************************************************************
 *   Copyright (C) 2014 by Aleix Pol Gonzalez <aleixpol@blue-systems.com>  *
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

import QtQuick 2.2
import org.kde.plasma.plasmoid 2.0
import org.kde.plasma.core 2.0 as PlasmaCore
import org.kde.plasma.PackageKit 1.0

Item
{
    Plasmoid.fullRepresentation: Full {}
    Plasmoid.toolTipSubText: PkUpdates.message
    Plasmoid.icon: PkUpdates.iconName

    property bool checkDaily: plasmoid.configuration.daily
    property bool checkWeekly: plasmoid.configuration.weekly
    property bool checkMonthly: plasmoid.configuration.monthly

    property bool checkOnMobile: plasmoid.configuration.check_on_mobile
    property bool checkOnBattery: plasmoid.configuration.check_on_battery

    readonly property int secsInDay: 60 * 60 * 24;
    readonly property int secsInWeek: secsInDay * 7;
    readonly property int secsInMonth: secsInDay * 30;

    Timer {
        id: timer
        repeat: true
        triggeredOnStart: true
        interval: 1000 * 60 * 60; // 1 hour
        onTriggered: {
            if (networkAllowed() && batteryAllowed())
                PkUpdates.checkUpdates(needsForcedUpdate())
        }
    }

    Binding {
        target: plasmoid
        property: "status"
        value: PkUpdates.isActive || !PkUpdates.isSystemUpToDate ? PlasmaCore.Types.ActiveStatus : PlasmaCore.Types.PassiveStatus;
    }

    Plasmoid.compactRepresentation: PlasmaCore.IconItem {
        source: PkUpdates.iconName
        width: 4
        height: 4
        MouseArea {
            anchors.fill: parent
            onClicked: {
                plasmoid.expanded = !plasmoid.expanded
            }
        }
    }

    function needsForcedUpdate() {
        var secs = PkUpdates.secondsSinceLastUpdate();
        if (secs === -1) { // never checked before
            return true;
        } else if (checkDaily) {
            return secs >= secsInDay;
        } else if (checkWeekly) {
            return secs >= secsInWeek;
        } else if (checkMonthly) {
            return secs >= secsInMonth;
        }
        return false;
    }

    function networkAllowed() {
        return PkUpdates.isNetworkMobile ? checkOnMobile : PkUpdates.isNetworkOnline
    }

    function batteryAllowed() {
        return PkUpdates.isOnBattery ? checkOnBattery : true
    }

    Component.onCompleted: {
        PkUpdates.networkStateChanged.connect(timer.restart)
        PkUpdates.isOnBatteryChanged.connect(timer.restart)
        timer.start()
    }
}
