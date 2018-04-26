#ifndef __DIALOG_MANAGER_H__
#define __DIALOG_MANAGER_H__

#include <QDialog>
#include <QPoint>
#include <QRect>

class DialogManager : public QObject
{
   Q_OBJECT

public:
   explicit DialogManager(const QRect& mainWinowRect);
   ~DialogManager() noexcept = default;

   DialogManager(const DialogManager&) = delete;
   DialogManager& operator = (const DialogManager&) = delete;

   DialogManager(DialogManager&&) = delete;
   DialogManager& operator = (DialogManager&&) = delete;

   void adjustDialogPosition(QDialog *dialog);

private slots:
   void onDialogFinished(int result);

private:
   void reset();

private:
   QPoint         dialogOffset_;
   QRect          screenSize_;
   const QPoint   center_;
   unsigned int   nbActiveDlgs_ = 0;

   int      rowHeight_;
};

#endif // __DIALOG_MANAGER_H__