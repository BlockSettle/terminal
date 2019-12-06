/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __VERSION_CHECKER_H__
#define __VERSION_CHECKER_H__

#include <atomic>
#include <deque>
#include <functional>

#include <QNetworkAccessManager>
#include <QObject>
#include <QJsonObject>
#include <QUrl>

class QNetworkReply;

namespace bs {
   struct ChangeLog
   {
      QString versionString;
      std::vector<QString> newFeatures;
      std::vector<QString> bugFixes;

      bool IsValid() const {
         return !versionString.isEmpty();
      }
   };

   class VersionChecker : public QObject
   {
      Q_OBJECT

   public:
      VersionChecker(const QString &baseUrl);
      ~VersionChecker() override = default;

      bool loadLatestVersion();

      QString getLatestVersion() const;

      const std::vector<ChangeLog>& getChangeLog() const;

   signals:
      void latestVersionLoaded(bool weAreUpToDate);
      void failedToLoadVersion();

   private slots:
      void finishedReply(QNetworkReply *);

   private:
      ChangeLog LoadChangelog(const QJsonObject& jsonObject);

   private:
      bool sendRequest(const QUrl &url);
      bool processReply(const QByteArray &data);

   private:
      QNetworkAccessManager   nam_;
      QString                 baseUrl_;

      QString                 latestVer_;
      std::vector<ChangeLog>  changeLog_;
   };
}


#endif // __VERSION_CHECKER_H__
