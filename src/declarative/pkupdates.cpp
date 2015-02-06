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
#include <QIcon>

#include <KLocalizedString>
#include <KFormat>
#include <KNotification>
#include <Solid/PowerManagement>
#include <KIconLoader>

#include "pkupdates.h"
#include "PkStrings.h"

PkUpdates::PkUpdates(QObject *parent) :
    QObject(parent),
    m_updatesTrans(Q_NULLPTR),
    m_cacheTrans(Q_NULLPTR),
    m_installTrans(Q_NULLPTR)
{
    setStatusMessage(i18n("Idle"));

    connect(PackageKit::Daemon::global(), &PackageKit::Daemon::changed, this, &PkUpdates::onChanged);
    connect(PackageKit::Daemon::global(), &PackageKit::Daemon::updatesChanged, this, &PkUpdates::onUpdatesChanged);
    connect(PackageKit::Daemon::global(), &PackageKit::Daemon::networkStateChanged, this, &PkUpdates::networkStateChanged);

    connect(Solid::PowerManagement::notifier(), &Solid::PowerManagement::Notifier::appShouldConserveResourcesChanged,
            this, &PkUpdates::isOnBatteryChanged);
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
    if (m_installTrans) {
        m_installTrans->deleteLater();
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
    } else if (!isNetworkOnline()) {
        return i18n("Your system is offline");
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

bool PkUpdates::isActive() const
{
    return m_active;
}

QVariantMap PkUpdates::packages() const
{
    return m_updateList;
}

bool PkUpdates::isNetworkOnline() const
{
    qDebug() << "Is net online:" << (PackageKit::Daemon::networkState() > PackageKit::Daemon::Network::NetworkOffline);
    return (PackageKit::Daemon::networkState() > PackageKit::Daemon::Network::NetworkOffline);
}

bool PkUpdates::isNetworkMobile() const
{
    qDebug() << "Is net mobile:" << (PackageKit::Daemon::networkState() == PackageKit::Daemon::Network::NetworkMobile);
    return (PackageKit::Daemon::networkState() == PackageKit::Daemon::Network::NetworkMobile);
}

bool PkUpdates::isOnBattery() const
{
    qDebug() << "Is on battery:" << Solid::PowerManagement::appShouldConserveResources();
    return Solid::PowerManagement::appShouldConserveResources();
}

QString PkUpdates::timestamp() const
{
    int lastCheck = secondsSinceLastUpdate();

    if (lastCheck != -1)
        return i18n("Last updated: %1 ago", KFormat().formatSpelloutDuration(lastCheck * 1000));

    return i18n("Last updated: never");
}

void PkUpdates::checkUpdates(bool force)
{
    qDebug() << "Checking updates, forced:" << force;

    m_cacheTrans = PackageKit::Daemon::refreshCache(force);
    setActive(true);

    connect(m_cacheTrans, &PackageKit::Transaction::statusChanged, this, &PkUpdates::onStatusChanged);
    connect(m_cacheTrans, &PackageKit::Transaction::finished, this, &PkUpdates::onFinished);
    connect(m_cacheTrans, &PackageKit::Transaction::errorCode, this, &PkUpdates::onErrorCode);
    connect(m_cacheTrans, &PackageKit::Transaction::requireRestart, this, &PkUpdates::onRequireRestart);
}

qint64 PkUpdates::secondsSinceLastRefresh() const
{
    QDBusReply<uint> lastCheckReply = PackageKit::Daemon::getTimeSinceAction(PackageKit::Transaction::Role::RoleRefreshCache);
    if (lastCheckReply.isValid()) {
        qDebug() << "Seconds since last refresh: " << lastCheckReply.value();
        if (lastCheckReply.value() != UINT_MAX) // not never
            return lastCheckReply.value();
    }

    return -1;
}

qint64 PkUpdates::secondsSinceLastUpdate() const
{
    QDBusReply<uint> lastCheckReply = PackageKit::Daemon::getTimeSinceAction(PackageKit::Transaction::Role::RoleUpdatePackages);
    if (lastCheckReply.isValid()) {
        qDebug() << "Seconds since last update: " << lastCheckReply.value();
        if (lastCheckReply.value() != UINT_MAX) // not never
            return lastCheckReply.value();
    }

    return -1;
}

QString PkUpdates::packageName(const QString &pkgId)
{
    return PackageKit::Daemon::packageName(pkgId);
}

void PkUpdates::getUpdates()
{
    m_updatesTrans = PackageKit::Daemon::getUpdates();
    setActive(true);

    m_updateList.clear();
    m_importantList.clear();
    m_securityList.clear();

    connect(m_updatesTrans, &PackageKit::Transaction::statusChanged, this, &PkUpdates::onStatusChanged);
    connect(m_updatesTrans, &PackageKit::Transaction::finished, this, &PkUpdates::onFinished);
    connect(m_updatesTrans, &PackageKit::Transaction::errorCode, this, &PkUpdates::onErrorCode);
    connect(m_updatesTrans, &PackageKit::Transaction::package, this, &PkUpdates::onPackage);
    connect(m_updatesTrans, &PackageKit::Transaction::requireRestart, this, &PkUpdates::onRequireRestart);
}

void PkUpdates::installUpdates(const QStringList &packageIds)
{
    qDebug() << "Installing updates" << packageIds;

    m_installTrans = PackageKit::Daemon::updatePackages(packageIds);
    setActive(true);

    connect(m_installTrans, &PackageKit::Transaction::statusChanged, this, &PkUpdates::onStatusChanged);
    connect(m_installTrans, &PackageKit::Transaction::finished, this, &PkUpdates::onFinished);
    connect(m_installTrans, &PackageKit::Transaction::errorCode, this, &PkUpdates::onErrorCode);
    connect(m_installTrans, &PackageKit::Transaction::package, this, &PkUpdates::onPackageUpdating);
    connect(m_installTrans, &PackageKit::Transaction::requireRestart, this, &PkUpdates::onRequireRestart);
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
        setStatusMessage(PkStrings::status(trans->status(), trans->speed(), trans->downloadSizeRemaining()));
        setPercentage(trans->percentage());
    }
}

void PkUpdates::onPackage(PackageKit::Transaction::Info info, const QString &packageID, const QString &summary)
{
    qDebug() << "Got update package:" << packageID << ", summary:" << summary <<
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
    m_updateList.insert(packageID, summary);
}

void PkUpdates::onPackageUpdating(PackageKit::Transaction::Info info, const QString &packageID, const QString &summary)
{
    Q_UNUSED(summary)
    qDebug() << "Package updating:" << packageID <<
                ", type:" << PackageKit::Daemon::enumToString<PackageKit::Transaction>((int)info, "Info");
    setStatusMessage(PkStrings::infoPresent(info) + " " + PackageKit::Daemon::packageName(packageID));
    setPercentage(m_installTrans->percentage());
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
            setActive(false);
            return;
        } else {
            qDebug() << "Cache transaction didn't finish successfully";
            emit done();
        }
    } else if (trans->role() == PackageKit::Transaction::RoleGetUpdates) {
        if (status == PackageKit::Transaction::ExitSuccess) {
            qDebug() << "Check updates transaction finished successfully";
        } else {
            qDebug() << "Check updates transaction didn't finish successfully";
        }
        qDebug() << "Total number of updates: " << count();
        emit done();
    } else if (trans->role() == PackageKit::Transaction::RoleUpdatePackages) {
        if (status == PackageKit::Transaction::ExitSuccess) {
            qDebug() << "Update packages transaction finished successfully";
            KNotification::event(KNotification::Notification, i18n("Updates Installed"),
                                 i18np("Successfully instaled 1 update", "Successfully installed %1 updates", m_updateList.count()),
                                 KIconLoader::global()->loadIcon("system-software-update", KIconLoader::Desktop));
            m_updateList.clear();
        } else {
            qDebug() << "Update packages transaction didn't finish successfully";
        }
        setActive(false);
        return;
    }

    setActive(false);
    emit updatesChanged();
}

void PkUpdates::onErrorCode(PackageKit::Transaction::Error error, const QString &details)
{
    qWarning() << "PK error:" << details << "type:" << PackageKit::Daemon::enumToString<PackageKit::Transaction>((int)error, "Error");
    KNotification::event(KNotification::Error, i18n("Update Error"), PkStrings::error(error) + " " + PkStrings::errorMessage(error),
                         KIconLoader::global()->loadIcon("system-software-update", KIconLoader::Desktop));
}

void PkUpdates::onRequireRestart(PackageKit::Transaction::Restart type, const QString &packageID)
{
    qDebug() << "RESTART" << PackageKit::Daemon::enumToString<PackageKit::Transaction>((int)type, "Restart")
             << "is required for package" << packageID;
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
