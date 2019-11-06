#ifndef __SUPPORT_DIALOG_H__
#define __SUPPORT_DIALOG_H__

#include "ui_SupportDialog.h"

#include <QDialog>
#include <QMainWindow>
#include <memory>

namespace Ui {
    class SupportDialog;
}

class SupportDialog : public QDialog
{
Q_OBJECT

public:
   SupportDialog(QWidget* parent = nullptr);
   ~SupportDialog() override;

   void setTab(int index) { ui_->stackedWidget->setCurrentIndex(index); }

private:
   std::unique_ptr<Ui::SupportDialog> ui_;
};

#endif // __SUPPORT_DIALOG_H__
