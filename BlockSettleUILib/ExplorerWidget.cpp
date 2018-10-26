#include "explorerwidget.h"
#include "ui_explorerwidget.h"
#include <QStringListModel>

ExplorerWidget::ExplorerWidget(QWidget *parent) :
	TabWithShortcut(parent),
    ui(new Ui::ExplorerWidget)
{
    ui->setupUi(this);

	//m_testModel = new QStringListModel();
	//QStringList list;
	//list << "a" << "b" << "c";
	//model->setStringList(list);
}

ExplorerWidget::~ExplorerWidget() = default;

void ExplorerWidget::shortcutActivated(ShortcutType s)
{
	switch (s) {
	case ShortcutType::Alt_1: {

	}
							  break;

	case ShortcutType::Alt_2: {

	}
							  break;

	default:
		break;
	}
}