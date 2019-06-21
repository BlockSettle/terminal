#ifndef EDITCONTACTDIALOG_H
#define EDITCONTACTDIALOG_H

#include <QDialog>

#include <memory>

namespace Ui {
class EditContactDialog;
}

class EditContactDialog : public QDialog
{
   Q_OBJECT

public:
   explicit EditContactDialog(QWidget *parent = nullptr);
   ~EditContactDialog() noexcept override;

private:
   std::unique_ptr<Ui::EditContactDialog> ui_;
};

#endif // EDITCONTACTDIALOG_H
