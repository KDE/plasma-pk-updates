/*
 *  Copyright 2015 Lukáš Tinkl <lukas@kde.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  2.010-1301, USA.
 */

import QtQuick 2.2
import QtQuick.Controls 1.3

import org.kde.plasma.core 2.0 as PlasmaCore
import org.kde.plasma.components 2.0 as PlasmaComponents

Item {
    id: iconsPage
    width: childrenRect.width
    height: childrenRect.height
    implicitWidth: pageColumn.implicitWidth
    implicitHeight: pageColumn.implicitHeight


    property alias cfg_daily: daily.checked
    property alias cfg_weekly: weekly.checked
    property alias cfg_monthly: monthly.checked

    Column {
        id: pageColumn
        spacing: units.smallSpacing
        GroupBox {
            ExclusiveGroup { id: intervalGroup }
            title: i18n("Check Interval")
            Column {
                spacing: units.smallSpacing
                RadioButton {
                    id: daily
                    text: i18n("Daily")
                    exclusiveGroup: intervalGroup
                }
                RadioButton {
                    id: weekly
                    text: i18n("Weekly")
                    exclusiveGroup: intervalGroup
                }
                RadioButton {
                    id: monthly
                    text: i18n("Monthly")
                    exclusiveGroup: intervalGroup
                }
            }
        }
    }
}
