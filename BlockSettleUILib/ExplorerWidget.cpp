#include "explorerwidget.h"
#include "ui_explorerwidget.h"
#include <QStringListModel>

ExplorerWidget::ExplorerWidget(QWidget *parent) :
	TabWithShortcut(parent),
    ui(new Ui::ExplorerWidget)
{
    ui->setupUi(this);

	connect(ui->searchBox, &QLineEdit::returnPressed, this, &ExplorerWidget::onSearchStarted);
	
}

ExplorerWidget::~ExplorerWidget() = default;

void ExplorerWidget::shortcutActivated(ShortcutType s)
{
	switch (s) {

	default:
		break;
	}
}

void ExplorerWidget::onSearchStarted()
{
	ui->stackedWidget->count();
	int index = ui->stackedWidget->currentIndex();
	if (index < ui->stackedWidget->count() - 1)	{
		ui->stackedWidget->setCurrentIndex(++index);
	}
	else {
		ui->stackedWidget->setCurrentIndex(0);
	}
}