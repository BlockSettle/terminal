/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __DIALOG_MANAGER_H__
#define __DIALOG_MANAGER_H__

#include <QDialog>
#include <QPoint>
#include <QList>
#include <QPointer>

class QWidget;
class DialogManager : public QObject
{
   Q_OBJECT

public:
   explicit DialogManager(const QWidget* mainWindow);
   ~DialogManager() noexcept = default;

   DialogManager(const DialogManager&) = delete;
   DialogManager& operator = (const DialogManager&) = delete;

   DialogManager(DialogManager&&) = delete;
   DialogManager& operator = (DialogManager&&) = delete;

   void adjustDialogPosition(QDialog *dialog);

private slots:
   void onDialogFinished();

private:
   bool prepare(QDialog* dlg);
   const QRect getGeometry(const QWidget* widget) const;
private:
   const QPointer<const QWidget> mainWindow_ = nullptr;
   QList<QPointer<QDialog>>      activeDlgs_;
};

#endif // __DIALOG_MANAGER_H__
