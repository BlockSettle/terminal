/*

***********************************************************************************
* Copyright (C) 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "OpenURIDialog.h"
#include "ui_OpenURIDialog.h"

#include <QLineEdit>
#include <QPushButton>

#include <spdlog/spdlog.h>


OpenURIDialog::OpenURIDialog(QWidget *parent)
   : QDialog(parent)
   , ui_(new Ui::OpenURIDialog())
{
   ui_->setupUi(this);

   connect(ui_->pushButtonOk, &QPushButton::clicked, this, &QDialog::accept);
   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &QDialog::reject);
}

OpenURIDialog::~OpenURIDialog() = default;
