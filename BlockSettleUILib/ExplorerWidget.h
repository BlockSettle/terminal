#ifndef EXPLORERWIDGET_H
#define EXPLORERWIDGET_H

#include <QWidget>
#include "TabWithShortcut.h"

namespace Ui {
class ExplorerWidget;
}

class QStringListModel;

class ExplorerWidget : public TabWithShortcut
{
    Q_OBJECT

public:
    ExplorerWidget(QWidget *parent = nullptr);
    ~ExplorerWidget() override;

	void shortcutActivated(ShortcutType s) override;

private:
	std::unique_ptr <Ui::ExplorerWidget> ui;

protected slots:
	void onSearchStarted();
};

#endif // EXPLORERWIDGET_H
