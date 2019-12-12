/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef EDITCONTACTDIALOG_H
#define EDITCONTACTDIALOG_H

#include <QDialog>
#include <QDateTime>

#include <memory>

namespace Ui {
class EditContactDialog;
}

class EditContactDialog : public QDialog
{
   Q_OBJECT

public:
   explicit EditContactDialog(
         const QString &contactId
         , const QString &displayName = QString()
         , const QDateTime &timestamp = QDateTime()
         , const QString &idKey = QString()
         , QWidget *parent = nullptr);
   ~EditContactDialog() noexcept override;

   QString contactId() const;
   QString displayName() const;
   QDateTime timestamp() const;
   QString idKey() const;

public slots:
   void accept() override;
   void reject() override;

protected:
   void showEvent(QShowEvent *event) override;

private:
   void refillFields();

private:
   std::unique_ptr<Ui::EditContactDialog> ui_;
   QString contactId_;
   QString displayName_;
   QDateTime timestamp_;
   QString idKey_;
};

#endif // EDITCONTACTDIALOG_H
