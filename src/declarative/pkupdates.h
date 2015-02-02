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


#ifndef PLASMA_PK_UPDATES_H
#define PLASMA_PK_UPDATES_H

#include <QObject>
#include <QPointer>

#include <PackageKit/Daemon>
#include <PackageKit/Transaction>

class QTimer;

/**
 * @brief The PkUpdates class
 *
 * Backend class to check for available PackageKit system updates.
 * Use checkUpdates() to perform the check.
 */
class PkUpdates : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY updatesChanged)
    Q_PROPERTY(int importantCount READ importantCount NOTIFY updatesChanged)
    Q_PROPERTY(int securityCount READ securityCount NOTIFY updatesChanged)
    Q_PROPERTY(bool isSystemUpToDate READ isSystemUpToDate NOTIFY updatesChanged)
    Q_PROPERTY(QString iconName READ iconName NOTIFY updatesChanged)
    Q_PROPERTY(QString message READ message NOTIFY isActiveChanged)
    Q_PROPERTY(int percentage READ percentage NOTIFY percentageChanged)
    Q_PROPERTY(QString timestamp READ timestamp NOTIFY updatesChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(QString packageNames READ packageNames NOTIFY updatesChanged)
    Q_PROPERTY(QStringList packageIds READ packageIds NOTIFY updatesChanged)
    Q_PROPERTY(bool isActive READ isActive NOTIFY isActiveChanged)

public:
    explicit PkUpdates(QObject *parent = 0);
    ~PkUpdates();

    /**
     * @return the total number of updates, including important and/or security ones
     */
    int count() const;

    /**
     * @return the number of important updates, included in total count()
     */
    int importantCount() const;

    /**
     * @return the number of security updates, included in total count()
     */
    int securityCount() const;

    /**
     * @return whether the system is up to date (count() == 0)
     */
    bool isSystemUpToDate() const;

    /**
     * @return the system update status icon name
     */
    QString iconName() const;

    /**
     * @return the overal status with number of available updates
     */
    QString message() const;

    /**
     * @return the progress percentage (0..100)
     */
    int percentage() const;

    /**
     * @return time stamp of the last update check
     */
    QString timestamp() const;

    /**
     * @return status messsage conveying the action being currently performed
     */
    QString statusMessage() const;

    /**
     * @return a comma separated (human readable) list of package names to update
     */
    QString packageNames() const;

    /**
     * @return whether we're currently checking for updates or not
     */
    bool isActive() const;

    /**
     * @return the IDs of the packages to update
     */
    QStringList packageIds() const;

signals:
    /**
     * Emitted when the number uf updates has changed
     */
    void updatesChanged();

    /**
     * Emitted when the updates check is finished (with success or error)
     */
    void done();

    // private ;)
    void statusMessageChanged();
    void timestampChanged();
    void isActiveChanged();
    void percentageChanged();

public slots:
    /**
      * Perform a cache update, possibly resulting in an update check. Signal updatesChanged() gets emitted
      * as a result. Consult the count() property whether there are new updates available.
      */
    Q_INVOKABLE void checkUpdates(bool force = false);

    /**
      * Launch the updater software to review and actually perform the update itself.
      */
    Q_INVOKABLE void reviewUpdates();

private slots:
    void getUpdates();
    void onChanged();
    void onUpdatesChanged();
    void onStatusChanged();
    void onPackage(PackageKit::Transaction::Info info, const QString &packageID, const QString &summary);
    void onFinished(PackageKit::Transaction::Exit status, uint runtime);
    void onErrorCode(PackageKit::Transaction::Error error, const QString &details);

private:
    void setStatusMessage(const QString &message);
    void setActive(bool active);
    void setPercentage(int value);
    QPointer<PackageKit::Transaction> m_updatesTrans;
    QPointer<PackageKit::Transaction> m_cacheTrans;
    QStringList m_updateList;
    QStringList m_importantList;
    QStringList m_securityList;
    QString m_statusMessage;
    bool m_active = false;
    int m_percentage = 0;
};

#endif // PLASMA_PK_UPDATES_H
