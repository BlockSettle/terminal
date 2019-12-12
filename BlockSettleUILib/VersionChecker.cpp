/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include "VersionChecker.h"

#include "TerminalVersion.h"

using namespace bs;

struct Version
{
   int major = -1;
   int minor = -1;
   int patch = -1;

   bool IsValid() const {
      return major != -1
         && minor  != -1
         && patch  != -1;
   }
};

bool operator < (const Version& l, const Version& r)
{
   if (l.major != r.major) {
      return l.major < r.major;
   }

   if (l.minor != r.minor) {
      return l.minor < r.minor;
   }

   return l.patch < r.patch;
}

QString VersionToString(const Version& version)
{
   return QString(QLatin1String("%1.%2.%3")).arg(version.major).arg(version.minor).arg(version.patch);
}

Version VersionFromString(const QString& versionString)
{
   auto stringList = versionString.split(QString::fromStdString("."), QString::SkipEmptyParts);

   Version version;

   if (stringList.size() == 3) {
      bool converted = false;

      version.major = stringList[0].toUInt(&converted);
      if (!converted) {
         return Version{};
      }

      version.minor = stringList[1].toUInt(&converted);
      if (!converted) {
         return Version{};
      }

      version.patch = stringList[2].toUInt(&converted);
      if (!converted) {
         return Version{};
      }
   }

   return version;
}

VersionChecker::VersionChecker(const QString &baseUrl)
   : baseUrl_(baseUrl)
{
   connect(&nam_, &QNetworkAccessManager::finished, this, &VersionChecker::finishedReply);
}

bool VersionChecker::loadLatestVersion()
{
   if (!sendRequest(baseUrl_)) {
      return false;
   }

   return true;
}

QString VersionChecker::getLatestVersion() const
{
   return latestVer_;
}

bool VersionChecker::processReply(const QByteArray &data)
{
   if (data.isNull() || data.isEmpty()) {
      return false;
   }

   QJsonParseError errJson;
   const auto doc = QJsonDocument::fromJson(data, &errJson);
   if (errJson.error != QJsonParseError::NoError) {
      return false;
   }
   const auto reply = doc.object();

   QString version;
   const auto versionIt = reply.find(QLatin1String("latest_version"));
   if (versionIt == reply.end()) {
      return false;
   }
   latestVer_ = versionIt.value().toString();

   Version currentVersion{TERMINAL_VERSION_MAJOR, TERMINAL_VERSION_MINOR, TERMINAL_VERSION_PATCH};
   Version receivedVersion = VersionFromString(latestVer_);

   if (currentVersion < receivedVersion) {
      const auto changesIt = reply.find(QLatin1String("changes"));
      if (changesIt != reply.end()) {
         auto changesList = changesIt.value().toArray();

         for (int i=0; i<changesList.size(); ++i) {
            auto changeLog  = LoadChangelog(changesList[i].toObject());
            const auto loadedVersion = VersionFromString(changeLog.versionString);
            if (!loadedVersion.IsValid()) {
               break;
            }

            if (currentVersion < loadedVersion) {
               changeLog_.emplace_back(std::move(changeLog));
            }
         }
      }

      emit latestVersionLoaded(false);
   } else {
      emit latestVersionLoaded(true);
   }

   return true;
};

ChangeLog VersionChecker::LoadChangelog(const QJsonObject& jsonObject)
{
   ChangeLog changeLog;
   auto versionIt = jsonObject.find(QLatin1String("version_string"));
   if (versionIt == jsonObject.end()) {
      return ChangeLog{};
   }

   changeLog.versionString = versionIt.value().toString();

   auto improvementsIt = jsonObject.find(QLatin1String("improvements"));
   if (improvementsIt != jsonObject.end()) {
      auto improvements = improvementsIt.value().toArray();
      for (int i = 0; i < improvements.size(); ++i) {
         changeLog.newFeatures.emplace_back(improvements[i].toString());
      }
   }

   auto fixesIt = jsonObject.find(QLatin1String("bug_fixes"));
   if (fixesIt != jsonObject.end()) {
      auto fixes = fixesIt.value().toArray();
      for (int i = 0; i < fixes.size(); ++i) {
         changeLog.bugFixes.emplace_back(fixes[i].toString());
      }
   }

   return changeLog;
}

bool VersionChecker::sendRequest(const QUrl &url)
{
   if (!url.isValid()) {
      return false;
   }

   return nam_.get(QNetworkRequest(url));
}

void VersionChecker::finishedReply(QNetworkReply *reply)
{
   if (reply->error()) {
      reply->deleteLater();
      processReply({});
      emit failedToLoadVersion();
      return;
   }
   const auto data = reply->readAll();
   if (!processReply(data)) {
      emit failedToLoadVersion();
   }

   reply->deleteLater();
}

const std::vector<ChangeLog>& VersionChecker::getChangeLog() const
{
   return changeLog_;
}
