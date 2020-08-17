/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef QT_GUI_ADAPTER_H
#define QT_GUI_ADAPTER_H

#include <set>
#include <QObject>
#include "ApiAdapter.h"
#include "ThreadSafeClasses.h"

namespace bs {
   namespace gui {
      namespace qt {
         class MainWindow;
      }
   }
}
namespace BlockSettle {
   namespace Terminal {
      class SettingsMessage_SettingsResponse;
   }
}
class BSTerminalSplashScreen;
class GuiThread;


using GuiQueue = ArmoryThreading::TimedQueue<bs::message::Envelope>;

class QtGuiAdapter : public QObject, public ApiBusAdapter, public bs::MainLoopRuner
{
   Q_OBJECT
   friend class GuiThread;
public:
   QtGuiAdapter(const std::shared_ptr<spdlog::logger> &);
   ~QtGuiAdapter() override;

   bool process(const bs::message::Envelope &) override;

   std::set<std::shared_ptr<bs::message::User>> supportedReceivers() const override {
      return { user_ };
   }
   std::string name() const override { return "QtGUI"; }

   void run(int &argc, char **argv) override;

private:
   bool processSettings(const bs::message::Envelope &env);
   bool processSettingsGetResponse(const BlockSettle::Terminal::SettingsMessage_SettingsResponse &);
   bool processAdminMessage(const bs::message::Envelope &env);
   void requestInitialSettings();
   void updateSplashProgress();
   void splashProgressCompleted();

private:
   std::shared_ptr<spdlog::logger>        logger_;
   std::shared_ptr<bs::message::UserTerminal>   userSettings_;
   bs::gui::qt::MainWindow * mainWindow_{ nullptr };
   BSTerminalSplashScreen  * splashScreen_{ nullptr };

   std::set<int>  createdComponents_;
   std::set<int>  loadingComponents_;
};


#endif	// QT_GUI_ADAPTER_H
