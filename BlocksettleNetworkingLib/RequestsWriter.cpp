#include "RequestsWriter.h"

#include <QDateTime>
#include <QDir>
#include <QFile>

#include <spdlog/spdlog.h>

RequestsWriter::RequestsWriter(const std::shared_ptr<spdlog::logger>& logger, const QString& requestsRoot)
   : logger_(logger)
   , requestsRoot_(requestsRoot)
{}

bool RequestsWriter::SaveRequest(const std::string& request, const std::string& requestName) const
{
   QString extension = QLatin1String(".bin");

   QString fileName;
   forever
   {
      QString dirName = GetCurrentRequestsDir();

      if (!QDir().mkpath(dirName)) {
         logger_->error("[RequestsWriter::SaveRequest] Failed to create dir {}."
            , dirName.toStdString());
         return false;
      }

      QString baseFileName = QString::number( QDateTime::currentMSecsSinceEpoch());
      fileName = QDir::cleanPath(dirName + QDir::separator() + baseFileName + extension);
      QFileInfo fileInfo(fileName);
      if (!fileInfo.exists()) {
         break;
      }
   }

   QFile outputFile(fileName);
   if (!outputFile.open(QIODevice::WriteOnly | QIODevice::Unbuffered)) {
      if (requestName.empty()) {
         logger_->error("[RequestsWriter::SaveRequest] Failed to create file {}."
            , fileName.toStdString());
      } else {
         logger_->error("[RequestsWriter::SaveRequest] Failed to create file {}. {} not saved."
            , fileName.toStdString(), requestName);
      }
      return false;
   }

   if (request.size() != outputFile.write(request.c_str(), request.length())) {
      if (requestName.empty()) {
         logger_->error("[RequestsWriter::SaveRequest] Failed to write request to file {}."
         , fileName.toStdString());
      } else {
         logger_->error("[RequestsWriter::SaveRequest] Failed to write request to file {}. {} not saved."
         , fileName.toStdString(), requestName);
      }
      return false;
   }

   outputFile.flush();
   outputFile.close();

   if (!requestName.empty()) {
      logger_->debug("[RequestsWriter::SaveRequest] {} saved to {}."
         , requestName, fileName.toStdString());
   }

   return true;
}

QString RequestsWriter::GetCurrentRequestsDir() const
{
   return QDir::cleanPath(requestsRoot_ + QDir::separator() + QDate::currentDate().toString( QLatin1String("dd_MM_yyyy") ));
}
