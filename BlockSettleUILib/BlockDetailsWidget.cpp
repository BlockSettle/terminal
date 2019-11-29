/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "BlockDetailsWidget.h"
#include "ui_BlockDetailsWidget.h"
#include <QIcon>


BlockDetailsWidget::BlockDetailsWidget(QWidget *parent) :
    QWidget(parent),
    ui_(new Ui::BlockDetailsWidget())
{
    ui_->setupUi(this);

    QIcon btcIcon(QLatin1String(":/FULL_LOGO"));

    ui_->icon->setPixmap(btcIcon.pixmap(80, 80));
}

BlockDetailsWidget::~BlockDetailsWidget() = default;
