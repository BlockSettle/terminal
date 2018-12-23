#include <QCoreApplication>

#include <iostream>

#include "ApplicationSettings.h"
#include "LogManager.h"
#include "ConnectionManager.h"

#include "ChatProtocol.h"
#include "ChatServer.h"


int main (int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    // load settings
    auto settings = std::make_shared<ApplicationSettings>();
    if (!settings->LoadApplicationSettings(app.arguments())) {
       std::cerr << "Error: Failed to parse command line arguments." << std::endl;
       return 1;
    }

    auto logManager = std::make_shared<bs::LogManager>();
    logManager->add(settings->GetLogsConfig());

    auto connectionManager = std::make_shared<ConnectionManager>(logManager->logger("message"));

    auto charServer = std::make_shared<ChatServer>(connectionManager, settings, logManager->logger("ChatServer"));

    charServer->startServer(settings->get<std::string>(ApplicationSettings::chatServerHost)
                            , settings->get<std::string>(ApplicationSettings::chatServerPort));

    return app.exec();
}
