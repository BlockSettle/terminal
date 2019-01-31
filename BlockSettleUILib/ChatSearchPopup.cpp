#include "chatsearchpopup.h"
#include "ui_chatsearchpopup.h"

#include <QLabel>
#include <QMenu>
#include <QAction>
#include <QtDebug>

ChatSearchPopup::ChatSearchPopup(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ChatSearchPopup)
{
    ui->setupUi(this);
    ui->label->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->label, &QLabel::customContextMenuRequested, this, &ChatSearchPopup::showMenu);
    _searchPopupMenu = new QMenu(this);
    QAction *addUserToContactsAction = _searchPopupMenu->addAction(QObject::tr("Add to contacts"));
    addUserToContactsAction->setStatusTip(QObject::tr("Click to add user to contact list"));
    connect(addUserToContactsAction, &QAction::triggered,
            [=](bool) { qDebug()<<"triggered"; emit addUserToContacts(ui->label->text()); }
    );
}

ChatSearchPopup::~ChatSearchPopup()
{
    //_searchPopupMenu->deleteLater();
    delete ui;
}

void ChatSearchPopup::setText(const QString &text)
{
    ui->label->setText(text);
}

void ChatSearchPopup::showMenu(const QPoint &pos)
{
    _searchPopupMenu->exec(mapToGlobal(pos));
}
