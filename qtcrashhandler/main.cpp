/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#include "mainwidget.h"
#include "dumpsender.h"

#include <QApplication>
#include <QFileInfo>
#include <QHostInfo>
#include <QProcess>
#include <QNetworkProxyFactory>
#include <QtPlugin>

Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin);

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);

#ifdef ENABLE_CRASH_HANDLER
	if (application.arguments().count() > 1) {
		const QString dumpPath = QApplication::arguments().at(1);
		if (!QFileInfo(dumpPath).exists())
			qWarning("dumpPath not exist");
	}
	else {
		qWarning("no dumpPath specified");
	}

    QNetworkProxyFactory::setUseSystemConfiguration(true);

    QHostInfo hostInfo = QHostInfo::fromName(QLatin1String("crashes.qt.io"));

	//if (hostInfo.error() != QHostInfo::NoError)
	//	return 0;

	QUrl submitUrl(QLatin1String("http://www.blocksettle.com/crash_report"));
    DumpSender dumpSender(submitUrl);

    MainWidget mainWindow;

    mainWindow.setProgressbarMaximum(dumpSender.dumperSize());

    QObject::connect(&mainWindow, &MainWidget::restartCrashedApplication,
                     &dumpSender, &DumpSender::restartCrashedApplication);
    QObject::connect(&mainWindow, &MainWidget::restartCrashedApplicationAndSendDump,
                     &dumpSender, &DumpSender::restartCrashedApplicationAndSendDump);
    QObject::connect(&mainWindow, &MainWidget::sendDump,
                     &dumpSender, &DumpSender::sendDumpAndQuit);
    QObject::connect(&mainWindow, &MainWidget::commentChanged,
                     &dumpSender, &DumpSender::setCommentText);
    QObject::connect(&mainWindow, &MainWidget::emailAdressChanged,
                     &dumpSender, &DumpSender::setEmailAddress);
    QObject::connect(&dumpSender, &DumpSender::uploadProgress,
                     &mainWindow, &MainWidget::updateProgressBar);

    mainWindow.show();

    int exit_code = application.exec();
#else
    int exit_code = 0;
#endif // ENABLE_CRASH_HANDLER

#ifdef Q_OS_WIN
    QProcess::startDetached(application.applicationDirPath() + QLatin1String("/CleanUpSystray.exe"));
#endif

    return exit_code;
}
