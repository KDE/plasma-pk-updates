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
#include <QDBusInterface>

#include <KLocalizedString>
#include <KFormat>
#include <KNotification>
#include <Solid/Power>
#include <Solid/AcPluggedJob>
#include <KConfigGroup>
#include <KSharedConfig>

#include "pkupdates.h"
#include "PkStrings.h"

Q_LOGGING_CATEGORY(PLASMA_PK_UPDATES, "plasma-pk-updates")

namespace
{
    const auto s_pkUpdatesIconName = QStringLiteral("system-software-update");
    const auto s_componentName = QStringLiteral("plasma_pk_updates");
    const auto s_eventIdUpdatesAvailable = QStringLiteral("updatesAvailable");
    const auto s_eventIdUpdatesInstalled = QStringLiteral("updatesInstalled");
    const auto s_eventIdRestartRequired = QStringLiteral("restartRequired");
    const auto s_eventIdError = QStringLiteral("updateError");
} // namespace {

PkUpdates::PkUpdates(QObject *parent) :
    QObject(parent),
    m_isOnBattery(true)
{
    setStatusMessage(i18n("Idle"));

    connect(PackageKit::Daemon::global(), &PackageKit::Daemon::changed, this, &PkUpdates::onChanged);
    connect(PackageKit::Daemon::global(), &PackageKit::Daemon::updatesChanged, this, &PkUpdates::onUpdatesChanged);
    connect(PackageKit::Daemon::global(), &PackageKit::Daemon::networkStateChanged, this, &PkUpdates::networkStateChanged);
    connect(Solid::Power::self(), &Solid::Power::resumeFromSuspend, this,
            [this] {PackageKit::Daemon::stateHasChanged(QStringLiteral("resume"));});

    connect(Solid::Power::self(), &Solid::Power::acPluggedChanged, this, [this] (bool acPlugged) {
            qCDebug(PLASMA_PK_UPDATES) << "acPluggedChanged onBattery:" << m_isOnBattery << "->" << !acPlugged;
            if (!acPlugged != m_isOnBattery) {
                m_isOnBattery = !acPlugged;
                emit PkUpdates::isOnBatteryChanged();
            }
    });
    auto acPluggedJob = Solid::Power::self()->isAcPlugged(this);
    connect(acPluggedJob , &Solid::Job::result, this, [this] (Solid::Job* job) {
        bool acPlugged = static_cast<Solid::AcPluggedJob*>(job)->isPlugged();
        qCDebug(PLASMA_PK_UPDATES) << "acPlugged initial state" << acPlugged;
        m_isOnBattery = !acPlugged;
        emit PkUpdates::isOnBatteryChanged();
    });
    acPluggedJob->start();

    connect(PackageKit::Daemon::global(), &PackageKit::Daemon::networkStateChanged, this, &PkUpdates::doDelayedCheckUpdates);
    connect(this, &PkUpdates::isActiveChanged, this, &PkUpdates::messageChanged);
    connect(this, &PkUpdates::networkStateChanged, this, &PkUpdates::messageChanged);
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
    if (securityCount() > 0) {
        return QStringLiteral("update-high");
    } else if (importantCount() > 0) {
        return QStringLiteral("update-medium");
    } else if (count() > 0) {
        return QStringLiteral("update-low");
    } else {
        return QStringLiteral("update-none");
    }
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
            return msg + "\n" + i18n("(including %1)", extra.join(i18n(" and ")));
    } else if (!isNetworkOnline()) {
        return i18n("Your system is offline");
    }

    switch (m_lastCheckState) {
    case NoCheckDone:
        return i18n("Not checked for updates yet");
    default:
    case CheckFailed:
        return i18n("Checking for updates failed");
    case CheckSucceeded:
        return i18n("Your system is up to date");
    }
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

void PkUpdates::doDelayedCheckUpdates()
{
    if (m_checkUpdatesWhenNetworkOnline && isNetworkOnline())
    {
        qCDebug(PLASMA_PK_UPDATES) << "CheckUpdates was delayed. Doing it now";
        m_checkUpdatesWhenNetworkOnline = false;
        checkUpdates(m_isManualCheck /* manual */);
    }
}

bool PkUpdates::isNetworkMobile() const
{
    qCDebug(PLASMA_PK_UPDATES) << "Is net mobile:" << (PackageKit::Daemon::networkState() == PackageKit::Daemon::Network::NetworkMobile);
    return (PackageKit::Daemon::networkState() == PackageKit::Daemon::Network::NetworkMobile);
}

bool PkUpdates::isOnBattery() const
{
    qCDebug(PLASMA_PK_UPDATES) << "Is on battery:" << m_isOnBattery;
    return m_isOnBattery;
}

void PkUpdates::getUpdateDetails(const QString &pkgID)
{
    qCDebug(PLASMA_PK_UPDATES) << "Requesting update details for" << pkgID;
    m_detailTrans = PackageKit::Daemon::getUpdateDetail(pkgID);
    connect(m_detailTrans.data(), &PackageKit::Transaction::updateDetail, this, &PkUpdates::onUpdateDetail);
}

QString PkUpdates::timestamp() const
{
    const qint64 lastCheck = QDateTime::currentMSecsSinceEpoch() - lastRefreshTimestamp();

    if (lastCheck != -1)
        return i18n("Last check: %1 ago", KFormat().formatSpelloutDuration(lastCheck));

    return i18n("Last check: never");
}

void PkUpdates::checkUpdates(bool manual)
{
    m_isManualCheck = manual;

    if (!isNetworkOnline())
    {
        qCDebug(PLASMA_PK_UPDATES) << "Checking updates delayed. Network is offline";
        m_checkUpdatesWhenNetworkOnline = true;
        return;
    }
    qCDebug(PLASMA_PK_UPDATES) << "Checking for updates";

    // ask the Packagekit daemon to refresh the cache
    m_cacheTrans = PackageKit::Daemon::refreshCache(false);
    setActivity(CheckingUpdates);

    // evaluate the result
    connect(m_cacheTrans.data(), &PackageKit::Transaction::statusChanged, this, &PkUpdates::onStatusChanged);
    connect(m_cacheTrans.data(), &PackageKit::Transaction::finished, this, &PkUpdates::onFinished);
    connect(m_cacheTrans.data(), &PackageKit::Transaction::errorCode, this, &PkUpdates::onRefreshErrorCode);
    connect(m_cacheTrans.data(), &PackageKit::Transaction::requireRestart, this, &PkUpdates::onRequireRestart);
    connect(m_cacheTrans.data(), &PackageKit::Transaction::repoSignatureRequired, this, &PkUpdates::onRepoSignatureRequired);
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

    connect(m_updatesTrans.data(), &PackageKit::Transaction::statusChanged, this, &PkUpdates::onStatusChanged);
    connect(m_updatesTrans.data(), &PackageKit::Transaction::finished, this, &PkUpdates::onFinished);
    connect(m_updatesTrans.data(), &PackageKit::Transaction::errorCode, this, &PkUpdates::onErrorCode);
    connect(m_updatesTrans.data(), &PackageKit::Transaction::package, this, &PkUpdates::onPackage);
    connect(m_updatesTrans.data(), &PackageKit::Transaction::requireRestart, this, &PkUpdates::onRequireRestart);
    connect(m_updatesTrans.data(), &PackageKit::Transaction::repoSignatureRequired, this, &PkUpdates::onRepoSignatureRequired);
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

    m_restartType = PackageKit::Transaction::RestartNone;
    m_requiredEulas.clear();
    m_packages = packageIds;
    m_installTrans = PackageKit::Daemon::updatePackages(m_packages, flags);
    setActivity(InstallingUpdates);

    connect(m_installTrans.data(), &PackageKit::Transaction::statusChanged, this, &PkUpdates::onStatusChanged);
    connect(m_installTrans.data(), &PackageKit::Transaction::finished, this, &PkUpdates::onFinished);
    connect(m_installTrans.data(), &PackageKit::Transaction::errorCode, this, &PkUpdates::onErrorCode);
    connect(m_installTrans.data(), &PackageKit::Transaction::package, this, &PkUpdates::onPackageUpdating);
    connect(m_installTrans.data(), &PackageKit::Transaction::requireRestart, this, &PkUpdates::onRequireRestart);
    connect(m_installTrans.data(), &PackageKit::Transaction::repoSignatureRequired, this, &PkUpdates::onRepoSignatureRequired);
    connect(m_installTrans.data(), &PackageKit::Transaction::eulaRequired, this, &PkUpdates::onEulaRequired);
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
        // Blocked updates are not installable updates so there is no
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

    if (percent <= 100) {
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
        m_lastCheckState = status == PackageKit::Transaction::ExitSuccess ? CheckSucceeded : CheckFailed;

        if (m_lastCheckState == CheckSucceeded) {
            qCDebug(PLASMA_PK_UPDATES) << "Cache transaction finished successfully";

            // save the timestamp
            KConfigGroup grp(KSharedConfig::openConfig("plasma-pk-updates"), "General");
            grp.writeEntry("Timestamp", QDateTime::currentDateTime().toMSecsSinceEpoch());
            grp.writeEntry("FailedAutoRefeshCount", 0);
            grp.sync();

            return;
        } else {
            qCDebug(PLASMA_PK_UPDATES) << "Cache transaction didn't finish successfully";
            emit done();
        }
    } else if (trans->role() == PackageKit::Transaction::RoleGetUpdates) {
        m_lastCheckState = status == PackageKit::Transaction::ExitSuccess ? CheckSucceeded : CheckFailed;

        if (m_lastCheckState == CheckSucceeded) {
            qCDebug(PLASMA_PK_UPDATES) << "Check updates transaction finished successfully";
            const int upCount = count();
            if (upCount != m_lastUpdateCount && m_lastNotification) {
                qCDebug(PLASMA_PK_UPDATES) << "Disposing old update count notification";
                m_lastNotification->close();
            }
            if (upCount > 0 && upCount != m_lastUpdateCount) {
                m_lastUpdateCount = upCount;
                m_lastNotification = KNotification::event(s_eventIdUpdatesAvailable,
                                     QString(),
                                     i18np("You have 1 new update", "You have %1 new updates", upCount),
                                     s_pkUpdatesIconName, nullptr, KNotification::Persistent,
                                     s_componentName);
                connect(m_lastNotification, &KNotification::closed, this, [this] {
                    qCDebug(PLASMA_PK_UPDATES) << "Old notification closed";
                    m_lastNotification = nullptr;
                    m_lastUpdateCount = 0;
                });
            }
        } else {
            qCDebug(PLASMA_PK_UPDATES) << "Check updates transaction didn't finish successfully";
        }
        qCDebug(PLASMA_PK_UPDATES) << "Total number of updates: " << count();
        emit done();
    } else if (trans->role() == PackageKit::Transaction::RoleUpdatePackages) {
        qCDebug(PLASMA_PK_UPDATES) << "Finished updating packages:" << m_packages;
        if (status == PackageKit::Transaction::ExitNeedUntrusted) {
            qCDebug(PLASMA_PK_UPDATES) << "Transaction needs untrusted packages";
            // restart transaction with "untrusted" flag
            installUpdates(m_packages, false /*simulate*/, true /*untrusted*/);
            return;
        } else if (status == PackageKit::Transaction::ExitEulaRequired) {
            qCDebug(PLASMA_PK_UPDATES) << "Acceptance of EULAs required";
            promptNextEulaAgreement();
            return;
        } else if (status == PackageKit::Transaction::ExitSuccess && trans->transactionFlags().testFlag(PackageKit::Transaction::TransactionFlagSimulate)) {
            qCDebug(PLASMA_PK_UPDATES) << "Simulation finished with success, restarting the transaction";
            installUpdates(m_packages, false /*simulate*/, false /*untrusted*/);
            return;
        } else if (status == PackageKit::Transaction::ExitSuccess) {
            qCDebug(PLASMA_PK_UPDATES) << "Update packages transaction finished successfully";
            if (m_lastNotification) {
                m_lastNotification->close();
            }
            KNotification::event(s_eventIdUpdatesInstalled,
                                 i18n("Updates Installed"),
                                 i18np("Successfully updated %1 package", "Successfully updated %1 packages", m_packages.count()),
                                 s_pkUpdatesIconName, nullptr,
                                 KNotification::CloseOnTimeout,
                                 s_componentName);
            showRestartNotification();
            emit updatesInstalled();
        } else {
            qCDebug(PLASMA_PK_UPDATES) << "Update packages transaction didn't finish successfully";
            // just try to refresh cache in case of error, the user might have installed the updates manually meanwhile
            checkUpdates(false /* manual */);
            return;
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
    showError(error, details);
}

void PkUpdates::onRefreshErrorCode(PackageKit::Transaction::Error error, const QString &details)
{
    if(!m_isManualCheck) {
        auto isTransientError = [] (PackageKit::Transaction::Error error) {
            return (error == PackageKit::Transaction::ErrorFailedInitialization) ||
                   (error == PackageKit::Transaction::ErrorNotAuthorized) ||
                   (error == PackageKit::Transaction::ErrorNoNetwork) ||
                   (error == PackageKit::Transaction::ErrorCannotGetLock);
        };

        KConfigGroup grp(KSharedConfig::openConfig("plasma-pk-updates"), "General");
        auto failCount = grp.readEntry<qint64>("FailedAutoRefeshCount", 0);
        failCount += 1;
        grp.writeEntry("FailedAutoRefeshCount", failCount);
        grp.sync();

        if(failCount <= 1 && isTransientError(error)) {
            qDebug(PLASMA_PK_UPDATES) << "Ignoring notification for likely transient error during automatic check";
            return;
        }
    }

    showError(error, details);
}

void PkUpdates::showError(PackageKit::Transaction::Error error, const QString &details)
{
    qWarning() << "PK error:" << details << "type:" << PackageKit::Daemon::enumToString<PackageKit::Transaction>((int)error, "Error");
    if (error == PackageKit::Transaction::ErrorBadGpgSignature || error == PackageKit::Transaction::ErrorNoLicenseAgreement)
        return;

    KNotification::event(s_eventIdError, i18n("Update Error"),
                         details,
                         s_pkUpdatesIconName, nullptr,
                         KNotification::Persistent,
                         s_componentName);
}

void PkUpdates::onRequireRestart(PackageKit::Transaction::Restart type, const QString &packageID)
{
    PackageKit::Transaction * trans = qobject_cast<PackageKit::Transaction *>(sender());
    if (!trans)
        return;

    qCDebug(PLASMA_PK_UPDATES) << "RESTART" << PackageKit::Daemon::enumToString<PackageKit::Transaction>((int)type, "Restart")
             << "is required for package" << packageID;

    // Only replace if the new type has a higher place in the list than the current one.
    static const PackageKit::Transaction::Restart restart_order[] = {
        PackageKit::Transaction::RestartNone,
        PackageKit::Transaction::RestartApplication,
        PackageKit::Transaction::RestartSession,
        PackageKit::Transaction::RestartSecuritySession,
        PackageKit::Transaction::RestartSystem,
        PackageKit::Transaction::RestartSecuritySystem,
    };

    auto prio_current = std::find(std::begin(restart_order), std::end(restart_order), m_restartType),
         prio_new = std::find(std::begin(restart_order), std::end(restart_order), type);

    if(prio_new == std::end(restart_order)) {
        qWarning() << "Unknown/unhandled restart type" << PackageKit::Daemon::enumToString<PackageKit::Transaction>((int)type, "Restart");
        return;
    }

    // Compare the priority
    if(std::distance(prio_current, prio_new) <= 0)
        return;

    m_restartType = type;
    m_restartPackageID = packageID;
}

void PkUpdates::showRestartNotification()
{
    KNotification *notification = nullptr;
    bool reboot = false;

    switch(m_restartType)
    {
    case PackageKit::Transaction::RestartSystem:
    case PackageKit::Transaction::RestartSecuritySystem:
        notification = new KNotification(s_eventIdRestartRequired, KNotification::Persistent);

        notification->setActions(QStringList{QLatin1String("Restart")});
        notification->setTitle(i18n("Restart is required"));
        notification->setText(i18n("The computer will have to be restarted after the update for the changes to take effect."));

        reboot = true;
        break;
    case PackageKit::Transaction::RestartSession:
    case PackageKit::Transaction::RestartSecuritySession:
        notification = new KNotification(s_eventIdRestartRequired, KNotification::Persistent);

        notification->setActions(QStringList{QLatin1String("Logout")});
        notification->setTitle(i18n("Session restart is required"));
        notification->setText(i18n("You will need to log out and back in after the update for the changes to take effect."));
        break;
    case PackageKit::Transaction::RestartNone:
    case PackageKit::Transaction::RestartApplication:
        return;
    default:
        qWarning() << "Unknown/unhandled restart type" << PackageKit::Daemon::enumToString<PackageKit::Transaction>((int)m_restartType, "Restart");
        return;
    }

    notification->setComponentName(s_componentName);
    notification->setIconName(s_pkUpdatesIconName);
    connect(notification, &KNotification::action1Activated, this, [reboot] () {
        QDBusInterface interface("org.kde.ksmserver", "/KSMServer", "org.kde.KSMServerInterface", QDBusConnection::sessionBus());
        if (reboot) {
            interface.asyncCall("logout", 0, 1, 2); // Options: do not ask again | reboot | force
        } else {
            interface.asyncCall("logout", 0, 0, 2); // Options: do not ask again | logout | force
        }
    });

    notification->sendEvent();
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

void PkUpdates::onEulaRequired(const QString &eulaID, const QString &packageID, const QString &vendor, const QString &licenseAgreement)
{
    m_requiredEulas[eulaID] = {packageID, vendor, licenseAgreement};
}

void PkUpdates::promptNextEulaAgreement()
{
    if(m_requiredEulas.empty()) {
        // Restart the transaction
        installUpdates(m_packages, false, false);
        return;
    }

    QString eulaID = m_requiredEulas.firstKey();
    const EulaData &eula = m_requiredEulas[eulaID];
    emit eulaRequired(eulaID, eula.packageID, eula.vendor, eula.licenseAgreement);
}

void PkUpdates::eulaAgreementResult(const QString &eulaID, bool agreed)
{
    if(!agreed) {
        qCDebug(PLASMA_PK_UPDATES) << "EULA declined";
        // Do the same as the failure case in onFinished
        checkUpdates(m_isManualCheck /* manual */);
        return;
    }

    m_eulaTrans = PackageKit::Daemon::acceptEula(eulaID);
    connect(m_eulaTrans.data(), &PackageKit::Transaction::finished, this,
            [this, eulaID] (PackageKit::Transaction::Exit exit, uint) {
                if (exit == PackageKit::Transaction::ExitSuccess) {
                    m_requiredEulas.remove(eulaID);
                    promptNextEulaAgreement();
                } else {
                    qCWarning(PLASMA_PK_UPDATES) << "EULA acceptance failed";
                }
            }
    );
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
