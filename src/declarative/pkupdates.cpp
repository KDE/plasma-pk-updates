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
#include <KNotification>
#include <Solid/PowerManagement>
#include <KIconLoader>
#include <KConfigGroup>
#include <KSharedConfig>

#include "pkupdates.h"
#include "PkStrings.h"

Q_LOGGING_CATEGORY(PLASMA_PK_UPDATES, "plasma-pk-updates")

PkUpdates::PkUpdates(QObject *parent) :
    QObject(parent),
    m_updatesTrans(Q_NULLPTR),
    m_cacheTrans(Q_NULLPTR),
    m_installTrans(Q_NULLPTR),
    m_detailTrans(Q_NULLPTR)
{
    setStatusMessage(i18n("Idle"));

    connect(PackageKit::Daemon::global(), &PackageKit::Daemon::changed, this, &PkUpdates::onChanged);
    connect(PackageKit::Daemon::global(), &PackageKit::Daemon::updatesChanged, this, &PkUpdates::onUpdatesChanged);
    connect(PackageKit::Daemon::global(), &PackageKit::Daemon::networkStateChanged, this, &PkUpdates::networkStateChanged);
    connect(Solid::PowerManagement::notifier(), &Solid::PowerManagement::Notifier::resumingFromSuspend, this,
            [this] {PackageKit::Daemon::stateHasChanged("resume");});

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
    if (m_detailTrans) {
        m_detailTrans->deleteLater();
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
    if (securityCount() > 0)
        return "security-low";
    else if (importantCount() > 0)
        return "security-medium";

    return "security-high";
}

QString PkUpdates::message() const
{
    if (isActive()) {
        if (m_activity == CheckingUpdates)
            return i18n("Checking updates");
        else if (m_activity == GettingUpdates)
            return i18n("Getting updates");
        else if (m_activity == InstallingUpdates)
            return i18n("Installing updates");

        return i18n("Working");
    } else if (!isSystemUpToDate()) {
        QStringList extra;
        const QString msg = i18np("You have 1 new update", "You have %1 new updates", count());
        if (securityCount() > 0)
            extra += i18np("1 security update", "%1 security updates", securityCount());
        if (importantCount() > 0)
            extra += i18np("1 important update", "%1 important updates", importantCount());

        if (extra.isEmpty())
            return msg;
        else
            return msg + "<br>" + i18n("(including %1)", extra.join(i18n(" and ")));
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
    return m_activity != Idle;
}

QVariantMap PkUpdates::packages() const
{
    return m_updateList;
}

bool PkUpdates::isNetworkOnline() const
{
    qCDebug(PLASMA_PK_UPDATES) << "Is net online:" << (PackageKit::Daemon::networkState() > PackageKit::Daemon::Network::NetworkOffline);
    return (PackageKit::Daemon::networkState() > PackageKit::Daemon::Network::NetworkOffline);
}

bool PkUpdates::isNetworkMobile() const
{
    qCDebug(PLASMA_PK_UPDATES) << "Is net mobile:" << (PackageKit::Daemon::networkState() == PackageKit::Daemon::Network::NetworkMobile);
    return (PackageKit::Daemon::networkState() == PackageKit::Daemon::Network::NetworkMobile);
}

bool PkUpdates::isOnBattery() const
{
    qCDebug(PLASMA_PK_UPDATES) << "Is on battery:" << Solid::PowerManagement::appShouldConserveResources();
    return Solid::PowerManagement::appShouldConserveResources();
}

void PkUpdates::getUpdateDetails(const QString &pkgID)
{
    qCDebug(PLASMA_PK_UPDATES) << "Requesting update details for" << pkgID;
    m_detailTrans = PackageKit::Daemon::getUpdateDetail(pkgID);
    connect(m_detailTrans, &PackageKit::Transaction::updateDetail, this, &PkUpdates::onUpdateDetail);
}

QString PkUpdates::timestamp() const
{
    const qint64 lastCheck = QDateTime::currentMSecsSinceEpoch() - lastRefreshTimestamp();

    if (lastCheck != -1)
        return i18n("Last update: %1 ago", KFormat().formatSpelloutDuration(lastCheck));

    return i18n("Last update: never");
}

void PkUpdates::checkUpdates(bool force)
{
    qCDebug(PLASMA_PK_UPDATES) << "Checking updates, forced:" << force;

    if (force) { // save the timestamp
        KConfigGroup grp(KSharedConfig::openConfig("plasma-pk-updates"), "General");
        grp.writeEntry("Timestamp", QDateTime::currentDateTime().toMSecsSinceEpoch());
        grp.sync();
    }

    m_cacheTrans = PackageKit::Daemon::refreshCache(force);
    setActivity(CheckingUpdates);

    connect(m_cacheTrans, &PackageKit::Transaction::statusChanged, this, &PkUpdates::onStatusChanged);
    connect(m_cacheTrans, &PackageKit::Transaction::finished, this, &PkUpdates::onFinished);
    connect(m_cacheTrans, &PackageKit::Transaction::errorCode, this, &PkUpdates::onErrorCode);
    connect(m_cacheTrans, &PackageKit::Transaction::requireRestart, this, &PkUpdates::onRequireRestart);
    connect(m_cacheTrans, &PackageKit::Transaction::repoSignatureRequired, this, &PkUpdates::onRepoSignatureRequired);
}

qint64 PkUpdates::lastRefreshTimestamp() const
{
    KConfigGroup grp(KSharedConfig::openConfig("plasma-pk-updates"), "General");
    return grp.readEntry<qint64>("Timestamp", -1);
}

QString PkUpdates::packageName(const QString &pkgId)
{
    return PackageKit::Daemon::packageName(pkgId);
}

QString PkUpdates::packageVersion(const QString &pkgId)
{
    return PackageKit::Daemon::packageVersion(pkgId);
}

void PkUpdates::getUpdates()
{
    m_updatesTrans = PackageKit::Daemon::getUpdates();
    setActivity(GettingUpdates);

    m_updateList.clear();
    m_importantList.clear();
    m_securityList.clear();

    connect(m_updatesTrans, &PackageKit::Transaction::statusChanged, this, &PkUpdates::onStatusChanged);
    connect(m_updatesTrans, &PackageKit::Transaction::finished, this, &PkUpdates::onFinished);
    connect(m_updatesTrans, &PackageKit::Transaction::errorCode, this, &PkUpdates::onErrorCode);
    connect(m_updatesTrans, &PackageKit::Transaction::package, this, &PkUpdates::onPackage);
    connect(m_updatesTrans, &PackageKit::Transaction::requireRestart, this, &PkUpdates::onRequireRestart);
    connect(m_updatesTrans, &PackageKit::Transaction::repoSignatureRequired, this, &PkUpdates::onRepoSignatureRequired);
}

void PkUpdates::installUpdates(const QStringList &packageIds, bool simulate, bool untrusted)
{
    qCDebug(PLASMA_PK_UPDATES) << "Installing updates" << packageIds << ", simulate:" << simulate << ", untrusted:" << untrusted;

    PackageKit::Transaction::TransactionFlags flags = PackageKit::Transaction::TransactionFlagOnlyTrusted;
    if (simulate) {
        flags |= PackageKit::Transaction::TransactionFlagSimulate;
    } else if (untrusted) {
        flags = PackageKit::Transaction::TransactionFlagNone;
    }

    m_installTrans = PackageKit::Daemon::updatePackages(packageIds, flags);
    m_installTrans->setProperty("packages", packageIds);
    setActivity(InstallingUpdates);

    connect(m_installTrans, &PackageKit::Transaction::statusChanged, this, &PkUpdates::onStatusChanged);
    connect(m_installTrans, &PackageKit::Transaction::finished, this, &PkUpdates::onFinished);
    connect(m_installTrans, &PackageKit::Transaction::errorCode, this, &PkUpdates::onErrorCode);
    connect(m_installTrans, &PackageKit::Transaction::package, this, &PkUpdates::onPackageUpdating);
    connect(m_installTrans, &PackageKit::Transaction::requireRestart, this, &PkUpdates::onRequireRestart);
    connect(m_installTrans, &PackageKit::Transaction::repoSignatureRequired, this, &PkUpdates::onRepoSignatureRequired);
}

void PkUpdates::onChanged()
{
    qCDebug(PLASMA_PK_UPDATES) << "Daemon changed";
}

void PkUpdates::onUpdatesChanged()
{
    qCDebug(PLASMA_PK_UPDATES) << "Updates changed, getting updates!";
    getUpdates();
}

void PkUpdates::onStatusChanged()
{
    PackageKit::Transaction * trans;
    if ((trans = qobject_cast<PackageKit::Transaction *>(sender()))) {
        qCDebug(PLASMA_PK_UPDATES) << "Transaction status changed:"
                 << PackageKit::Daemon::enumToString<PackageKit::Transaction>((int)trans->status(), "Status")
                 << QStringLiteral("(%1%)").arg(trans->percentage());
        if (trans->status() == PackageKit::Transaction::StatusFinished)
            return;
        setStatusMessage(PkStrings::status(trans->status(), trans->speed(), trans->downloadSizeRemaining()));
        setPercentage(trans->percentage());
    }
}

void PkUpdates::onPackage(PackageKit::Transaction::Info info, const QString &packageID, const QString &summary)
{
    qCDebug(PLASMA_PK_UPDATES) << "Got update package:" << packageID << ", summary:" << summary <<
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
    qCDebug(PLASMA_PK_UPDATES) << "Package updating:" << packageID <<
                ", info:" << PackageKit::Daemon::enumToString<PackageKit::Transaction>((int)info, "Info");

    const uint percent = m_installTrans->percentage();

    if (percent >= 0 && percent <= 100) {
        setStatusMessage(i18nc("1 installation status, 2 pkg name, 3 percentage", "%1 %2 (%3%)",
                               PkStrings::infoPresent(info), PackageKit::Daemon::packageName(packageID), percent));
    } else {
        setStatusMessage(i18nc("1 installation status, 2 pkg name", "%1 %2",
                               PkStrings::infoPresent(info), PackageKit::Daemon::packageName(packageID), percent));
    }

    setPercentage(percent);
}

void PkUpdates::onFinished(PackageKit::Transaction::Exit status, uint runtime)
{
    PackageKit::Transaction * trans = qobject_cast<PackageKit::Transaction *>(sender());
    if (!trans)
        return;

    trans->deleteLater();

    qCDebug(PLASMA_PK_UPDATES) << "Transaction" << trans->tid().path() <<
                "finished with status" << PackageKit::Daemon::enumToString<PackageKit::Transaction>((int)status, "Exit") <<
                "in" << runtime/1000 << "seconds";

    if (trans->role() == PackageKit::Transaction::RoleRefreshCache) {
        if (status == PackageKit::Transaction::ExitSuccess) {
            qCDebug(PLASMA_PK_UPDATES) << "Cache transaction finished successfully";
            return;
        } else {
            qCDebug(PLASMA_PK_UPDATES) << "Cache transaction didn't finish successfully";
            emit done();
        }
    } else if (trans->role() == PackageKit::Transaction::RoleGetUpdates) {
        if (status == PackageKit::Transaction::ExitSuccess) {
            qCDebug(PLASMA_PK_UPDATES) << "Check updates transaction finished successfully";
            const int upCount = count();
            if (upCount > 0) {
                KNotification::event(KNotification::Notification, i18n("Software Updates Available"),
                                     i18np("You have 1 new update", "You have %1 new updates", upCount),
                                     KIconLoader::global()->loadIcon("system-software-update", KIconLoader::Desktop), 0, KNotification::Persistent);
            }
        } else {
            qCDebug(PLASMA_PK_UPDATES) << "Check updates transaction didn't finish successfully";
        }
        qCDebug(PLASMA_PK_UPDATES) << "Total number of updates: " << count();
        emit done();
    } else if (trans->role() == PackageKit::Transaction::RoleUpdatePackages) {
        const QStringList packages = trans->property("packages").toStringList();
        qCDebug(PLASMA_PK_UPDATES) << "Finished updating packages:" << packages;
        if (status == PackageKit::Transaction::ExitNeedUntrusted) {
            qCDebug(PLASMA_PK_UPDATES) << "Transaction needs untrusted packages";
            // restart transaction with "untrusted" flag
            installUpdates(packages, false /*simulate*/, true /*untrusted*/);
            return;
        } else if (status == PackageKit::Transaction::ExitSuccess && trans->transactionFlags().testFlag(PackageKit::Transaction::TransactionFlagSimulate)) {
            qCDebug(PLASMA_PK_UPDATES) << "Simulation finished with success, restarting the transaction";
            installUpdates(packages, false /*simulate*/, false /*untrusted*/);
            return;
        } else if (status == PackageKit::Transaction::ExitSuccess) {
            qCDebug(PLASMA_PK_UPDATES) << "Update packages transaction finished successfully";
            KNotification::event(KNotification::Notification, i18n("Updates Installed"),
                                 i18np("Successfully updated %1 package", "Successfully updated %1 packages", packages.count()),
                                 KIconLoader::global()->loadIcon("system-software-update", KIconLoader::Desktop), 0, KNotification::Persistent);
        } else {
            qCDebug(PLASMA_PK_UPDATES) << "Update packages transaction didn't finish successfully";
        }
        setActivity(Idle);
        return;
    } else {
        qCDebug(PLASMA_PK_UPDATES) << "Unhandled transaction type:" << PackageKit::Daemon::enumToString<PackageKit::Transaction>(trans->role(), "Role");
        setActivity(Idle);
        return;
    }

    setActivity(Idle);
    emit updatesChanged();
}

void PkUpdates::onErrorCode(PackageKit::Transaction::Error error, const QString &details)
{
    qWarning() << "PK error:" << details << "type:" << PackageKit::Daemon::enumToString<PackageKit::Transaction>((int)error, "Error");
    if (error == PackageKit::Transaction::ErrorBadGpgSignature)
        return;

    KNotification::event(KNotification::Error, i18n("Update Error"), PkStrings::error(error) + " " + PkStrings::errorMessage(error),
                         KIconLoader::global()->loadIcon("system-software-update", KIconLoader::Desktop), 0, KNotification::Persistent);
}

void PkUpdates::onRequireRestart(PackageKit::Transaction::Restart type, const QString &packageID)
{
    qCDebug(PLASMA_PK_UPDATES) << "RESTART" << PackageKit::Daemon::enumToString<PackageKit::Transaction>((int)type, "Restart")
             << "is required for package" << packageID;
}

void PkUpdates::onUpdateDetail(const QString &packageID, const QStringList &updates, const QStringList &obsoletes,
                               const QStringList &vendorUrls, const QStringList &bugzillaUrls, const QStringList &cveUrls,
                               PackageKit::Transaction::Restart restart, const QString &updateText, const QString &changelog,
                               PackageKit::Transaction::UpdateState state, const QDateTime &issued, const QDateTime &updated)
{
    Q_UNUSED(updates);
    Q_UNUSED(obsoletes);
    Q_UNUSED(vendorUrls);
    Q_UNUSED(cveUrls);
    Q_UNUSED(restart);
    Q_UNUSED(changelog);
    Q_UNUSED(state);
    Q_UNUSED(issued);
    Q_UNUSED(updated);

    qCDebug(PLASMA_PK_UPDATES) << "Got update details for" << packageID;

    emit updateDetail(packageID, updateText, bugzillaUrls);
}

void PkUpdates::onRepoSignatureRequired(const QString &packageID, const QString &repoName, const QString &keyUrl, const QString &keyUserid,
                                        const QString &keyId, const QString &keyFingerprint, const QString &keyTimestamp,
                                        PackageKit::Transaction::SigType type)
{
    Q_UNUSED(repoName);
    Q_UNUSED(keyUrl);
    Q_UNUSED(keyUserid);
    Q_UNUSED(keyId);
    Q_UNUSED(keyFingerprint);
    Q_UNUSED(keyTimestamp);
    Q_UNUSED(type);

    // TODO provide a way to confirm and import GPG keys
    qCDebug(PLASMA_PK_UPDATES) << "Repo sig required" << packageID;
}

void PkUpdates::setStatusMessage(const QString &message)
{
    m_statusMessage = message;
    emit statusMessageChanged();
}

void PkUpdates::setActivity(Activity act)
{
    if (act != m_activity) {
        m_activity = act;
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
