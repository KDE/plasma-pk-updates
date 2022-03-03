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
import QtQuick.Controls 2.5

import org.kde.kirigami 2.5 as Kirigami

Kirigami.FormLayout {
    id: iconsPage

    anchors.left: parent.left
    anchors.right: parent.right

    property alias cfg_daily: daily.checked
    property alias cfg_weekly: weekly.checked
    property alias cfg_monthly: monthly.checked
    property alias cfg_check_on_mobile: mobile.checked
    property alias cfg_check_on_battery: battery.checked

    ButtonGroup {
        id: intervalGroup
    }

    RadioButton {
        id: daily
        Kirigami.FormData.label: i18nc("@label check interval for updates", "Check interval:")
        ButtonGroup.group: intervalGroup
        text: i18n("Daily")
    }

    RadioButton {
        id: weekly
        ButtonGroup.group: intervalGroup
        text: i18n("Weekly")
    }

    RadioButton {
        id: monthly
        ButtonGroup.group: intervalGroup
        text: i18n("Monthly")
    }

    CheckBox {
        id: mobile
        Kirigami.FormData.label: i18nc("@label part of a sentence", "Check for updates when:")
        text: i18nc("@option:check part of a sentence: Check for updates when", "On a mobile connection")
    }

    CheckBox {
        id: battery
        text: i18nc("@option:check part of a sentence: Check for updates when", "On battery")
    }
}
