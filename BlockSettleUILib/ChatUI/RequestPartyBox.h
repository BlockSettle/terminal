/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef REQUESTPARTY_H
#define REQUESTPARTY_H

#include <QDialog>
#include <memory>
#include "ui_RequestPartyBox.h"

class RequestPartyBox : public QDialog
{
   Q_OBJECT

public:
   RequestPartyBox(const QString& title, const QString& note, QWidget* parent = nullptr);
   QString getCustomMessage() const;

private:
   QString title_;
   QString note_;
   std::unique_ptr<Ui::RequestPartyBox> ui_;
};

#endif // REQUESTPARTY_H
