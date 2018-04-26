#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include "VersionChecker.h"

#include "TerminalVersion.h"

using namespace bs;

struct Version
{
   int major;
   int minor;
   int patch;
};

bool operator < (const Version& l, const Version& r)
{
   int lVersion = l.major * 100 + l.minor * 10 + l.patch;
   int rVersion = r.major * 100 + r.minor * 10 + r.patch;

   return lVersion < rVersion;
}

QString VersionToString(const Version& version)
{
   return QString(QLatin1String("%1.%2.%3")).arg(version.major).arg(version.minor).arg(version.patch);
}

VersionChecker::VersionChecker(const QString &baseUrl)
   : baseUrl_(baseUrl)
{
   connect(&nam_, &QNetworkAccessManager::finished, this, &VersionChecker::finishedReply);
}

bool VersionChecker::loadLatestVersion()
{
   if (!sendRequest(baseUrl_ + QLatin1String("/latest"))) {
      return false;
   }

   return true;
}

QString VersionChecker::getLatestVersion() const
{
   if (changeLog_.empty()) {
      return QString::fromLatin1(TERMINAL_VERSION_STRING);
   }
   return changeLog_[0].versionString;
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
   const auto versionIt = reply.find(QLatin1String("version"));
   if (versionIt == reply.end()) {
      return false;
   }
   const auto versionObject = versionIt.value().toObject();

   int major = 0;
   int minor = 0;
   int patch = 0;

   const auto majorIt = versionObject.find(QLatin1String("major"));
   if (majorIt == versionObject.end()) {
      return false;
   }
   major = majorIt.value().toInt();

   const auto minorIt = versionObject.find(QLatin1String("minor"));
   if (minorIt == versionObject.end()) {
      return false;
   }
   minor = minorIt.value().toInt();

   const auto patchIt = versionObject.find(QLatin1String("patch"));
   if (patchIt == versionObject.end()) {
      return false;
   }
   patch = patchIt.value().toInt();

   Version currentVersion{TERMINAL_VERSION_MAJOR, TERMINAL_VERSION_MINOR, TERMINAL_VERSION_PATCH};
   Version receivedVersion{major, minor, patch};

   if (currentVersion < receivedVersion) {
      ChangeLog loadedVersionLog;
      loadedVersionLog.versionString = VersionToString(receivedVersion);

      const auto changelogIt = reply.find(QLatin1String("changelog"));
      if (changelogIt == reply.end()) {
         return false;
      }
      auto changelogObject = changelogIt.value().toObject();

      const auto newFeaturesIt = changelogObject.find(QLatin1String("New features"));
      if (newFeaturesIt != changelogObject.end()) {
         for (const auto& feature : newFeaturesIt.value().toArray()) {
            loadedVersionLog.newFeatures.emplace_back(feature.toString());
         }
      }

      const auto bugFixes = changelogObject.find(QLatin1String("Bug fixes"));
      if (bugFixes != changelogObject.end()) {
         for (const auto& bugFix : bugFixes.value().toArray()) {
            loadedVersionLog.bugFixes.emplace_back(bugFix.toString());
         }
      }

      changeLog_.emplace_back(std::move(loadedVersionLog));

      // load prev version
      const auto prevVerIt = reply.find(QLatin1String("prev_version"));
      if (prevVerIt == reply.end()) {
         emit latestVersionLoaded(false);
         return true;
      }

      const QString prevVersion = prevVerIt.value().toString();
      if (prevVersion == VersionToString(currentVersion)) {
         emit latestVersionLoaded(false);
         return true;
      }

      return sendRequest(baseUrl_ + QLatin1String("/") + prevVersion);
   } else {
      emit latestVersionLoaded(changeLog_.empty());
   }

   return true;
};

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
      return;
   }
   const auto data = reply->readAll();
   processReply(data);
   reply->deleteLater();
}

const std::vector<ChangeLog>& VersionChecker::getChangeLog() const
{
   return changeLog_;
}
