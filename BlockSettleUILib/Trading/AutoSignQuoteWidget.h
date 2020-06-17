/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef AUTOSIGNQUOTEWIDGET_H
#define AUTOSIGNQUOTEWIDGET_H

#include "BSErrorCode.h"

#include <QWidget>
#include <memory>

class AutoSignScriptProvider;

namespace Ui {
class AutoSignQuoteWidget;
}

class AutoSignQuoteWidget : public QWidget
{
   Q_OBJECT

public:
   explicit AutoSignQuoteWidget(QWidget *parent = nullptr);
   ~AutoSignQuoteWidget();

   void init(const std::shared_ptr<AutoSignScriptProvider> &autoSignQuoteProvider);

public slots:
   void onAutoSignStateChanged();
   void onAutoSignReady();

private slots:
   void fillScriptHistory();
   void scriptChanged(int curIndex);

   void onAutoQuoteToggled();
   void onAutoSignToggled();

private:
   QString askForScript();
   void validateGUI();

private:
   std::unique_ptr<Ui::AutoSignQuoteWidget>      ui_;
   std::shared_ptr<AutoSignScriptProvider>       autoSignProvider_;
};

#endif // AUTOSIGNQUOTEWIDGET_H
