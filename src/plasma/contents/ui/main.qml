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

    Timer {
        id: timer
        repeat: true
        triggeredOnStart: true
        interval: 1000 * 60 * 60 * 24; // 1 day
        onTriggered: PkUpdates.checkUpdates()
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

    Component.onCompleted: {
        timer.start()
    }
}
