/***************************************************************************
 *   Copyright (C) 2015 Lukáš Tinkl <lukas@kde.org>                        *
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
 *   along with this program; see the file COPYING. If not, write to       *
 *   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,  *
 *   Boston, MA 02110-1301, USA.                                           *
 ***************************************************************************/

#include <limits.h>

#include <QDebug>
#include <QTimer>
#include <QDBusReply>

#include <KLocalizedString>
#include <KFormat>
#include <KConfigCore/KConfigGroup>

#include "pkupdates.h"
#include "PkStrings.h"

PkUpdates::PkUpdates(QObject *parent) :
    QObject(parent),
    m_updatesTrans(Q_NULLPTR),
    m_cacheTrans(Q_NULLPTR)
{
    setStatusMessage(i18n("Idle"));

    connect(PackageKit::Daemon::global(), &PackageKit::Daemon::changed, this, &PkUpdates::onChanged);
    connect(PackageKit::Daemon::global(), &PackageKit::Daemon::updatesChanged, this, &PkUpdates::onUpdatesChanged);
}

PkUpdates::~PkUpdates()
{
    if (m_cacheTrans) {
        if (m_cacheTrans->allowCancel())
            m_cacheTrans->cancel();
        m_cacheTrans->deleteLater();
    }
    if (m_updatesTrans) {
        if (m_updatesTrans->allowCancel())
            m_updatesTrans->cancel();
        m_updatesTrans->deleteLater();
    }
}

int PkUpdates::count() const
{
    return m_updateList.count();
}

int PkUpdates::importantCount() const
{
    return m_importantList.count();
}

int PkUpdates::securityCount() const
{
    return m_securityList.count();
}

bool PkUpdates::isSystemUpToDate() const
{
    return m_updateList.isEmpty();
}

QString PkUpdates::iconName() const
{
    if (securityCount()>0)
        return "security-low";
    else if (importantCount() > 0)
        return "security-medium";

    return "security-high";
}

QString PkUpdates::message() const
{
    if (!isSystemUpToDate()) {
        QStringList extra;
        const QString msg = i18np("You have 1 new update", "You have %1 new updates", count());
        if (securityCount()>0)
            extra += i18np("1 security update", "%1 security updates", securityCount());
        if (importantCount() > 0)
            extra += i18np("1 important update", "%1 important updates", importantCount());

        if (extra.isEmpty())
            return msg;
        else
            return msg + "<br>" + i18n("(including %1)", extra.join(i18n(" and ")));
    } else if (isActive()) {
        return i18n("Checking for updates");
    }

    return i18n("Your system is up to date");
}

int PkUpdates::percentage() const
{
    return m_percentage;
}

QString PkUpdates::statusMessage() const
{
    return m_statusMessage;
}

QString PkUpdates::packageNames() const
{
    QStringList tmp;
    foreach (const QString & entry, m_updateList) {
        tmp.append(PackageKit::Daemon::packageName(entry));
    }
    return tmp.join(", ");
}

bool PkUpdates::isActive() const
{
    return m_active;
}

QStringList PkUpdates::packageIds() const
{
    return m_updateList;
}

QString PkUpdates::timestamp() const
{
    QDBusReply<uint> lastCheckReply = PackageKit::Daemon::getTimeSinceAction(PackageKit::Transaction::Role::RoleRefreshCache);
    if (lastCheckReply.isValid()) {
        qDebug() << "Last cache check was" << lastCheckReply.value() << "seconds ago";

        if (lastCheckReply.value() != UINT_MAX) // not never
            return i18n("Last checked: %1 ago", KFormat().formatSpelloutDuration(lastCheckReply.value() * 1000));
    }

    return i18n("Last checked: never");
}

void PkUpdates::checkUpdates(bool force)
{
    qDebug() << "Checking updates, forced:" << force;

    m_updateList.clear();
    m_importantList.clear();
    m_securityList.clear();

    m_cacheTrans = PackageKit::Daemon::refreshCache(force);
    setActive(true);

    connect(m_cacheTrans, &PackageKit::Transaction::statusChanged, this, &PkUpdates::onStatusChanged);
    connect(m_cacheTrans, &PackageKit::Transaction::finished, this, &PkUpdates::onFinished);
    connect(m_cacheTrans, &PackageKit::Transaction::errorCode, this, &PkUpdates::onErrorCode);
}

void PkUpdates::reviewUpdates()
{
    // TODO
}

void PkUpdates::getUpdates()
{
    m_updatesTrans = PackageKit::Daemon::getUpdates();
    setActive(true);

    connect(m_updatesTrans, &PackageKit::Transaction::statusChanged, this, &PkUpdates::onStatusChanged);
    connect(m_updatesTrans, &PackageKit::Transaction::finished, this, &PkUpdates::onFinished);
    connect(m_updatesTrans, &PackageKit::Transaction::errorCode, this, &PkUpdates::onErrorCode);
    connect(m_updatesTrans, &PackageKit::Transaction::package, this, &PkUpdates::onPackage);
}

void PkUpdates::onChanged()
{
    qDebug() << "Daemon changed";
}

void PkUpdates::onUpdatesChanged()
{
    qDebug() << "Updates changed, getting updates!";
    getUpdates();
}

void PkUpdates::onStatusChanged()
{
    PackageKit::Transaction * trans;
    if ((trans = qobject_cast<PackageKit::Transaction *>(sender()))) {
        qDebug() << "Transaction status changed:"
                 << PackageKit::Daemon::enumToString<PackageKit::Transaction>((int)trans->status(), "Status")
                 << QStringLiteral("(%1%)").arg(trans->percentage());
        setStatusMessage(PkStrings::status(trans->status()));
        setPercentage(trans->percentage());
    }
}

void PkUpdates::onPackage(PackageKit::Transaction::Info info, const QString &packageID, const QString &summary)
{
    qDebug() << "Got package:" << packageID << ", summary:" << summary <<
                ", type:" << PackageKit::Daemon::enumToString<PackageKit::Transaction>((int)info, "Info");

    switch (info) {
    case PackageKit::Transaction::InfoBlocked:
        // Blocked updates are not instalable updates so there is no
        // reason to show/count them
        return;
    case PackageKit::Transaction::InfoImportant:
        m_importantList << packageID;
        break;
    case PackageKit::Transaction::InfoSecurity:
        m_securityList << packageID;
        break;
    default:
        break;
    }
    m_updateList << packageID;
}

void PkUpdates::onFinished(PackageKit::Transaction::Exit status, uint runtime)
{
    PackageKit::Transaction * trans = qobject_cast<PackageKit::Transaction *>(sender());
    if (!trans)
        return;

    qDebug() << "Transaction" << trans->tid().path() <<
                "finished with status" << PackageKit::Daemon::enumToString<PackageKit::Transaction>((int)status, "Exit") <<
                "in" << runtime/1000 << "seconds";

    if (trans->role() == PackageKit::Transaction::RoleRefreshCache) {
        if (status == PackageKit::Transaction::ExitSuccess) {
            qDebug() << "Cache transaction finished successfully";
            return;
        } else {
            qDebug() << "Cache transaction didn't finish successfully";
        }
    } else if (trans->role() == PackageKit::Transaction::RoleGetUpdates) {
        if (status == PackageKit::Transaction::ExitSuccess) {
            qDebug() << "Check updates transaction finished successfully";
        } else {
            qDebug() << "Check updates transaction didn't finish successfully";
        }
        qDebug() << "Total number of updates: " << count();
        emit done();
    }

    setActive(false);
    emit updatesChanged();
}

void PkUpdates::onErrorCode(PackageKit::Transaction::Error error, const QString &details)
{
    qWarning() << "PK error:" << details << "type:" << PackageKit::Daemon::enumToString<PackageKit::Transaction>((int)error, "Error");
    setStatusMessage(PkStrings::error(error) + "<br>" + details);
}

void PkUpdates::setStatusMessage(const QString &message)
{
    m_statusMessage = message;
    emit statusMessageChanged();
}

void PkUpdates::setActive(bool active)
{
    if (active != m_active) {
        m_active = active;
        emit isActiveChanged();
    }
}

void PkUpdates::setPercentage(int value)
{
    if (value != m_percentage) {
        m_percentage = value;
        emit percentageChanged();
    }
}
