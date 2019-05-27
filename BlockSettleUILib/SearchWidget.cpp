#include "SearchWidget.h"
#include "ui_SearchWidget.h"
#include "ChatProtocol/DataObjects/UserData.h"

#include <QTimer>

/**
 * @brief Model for testing. Should be replaced after debug
 */

class ListModel : public QAbstractListModel
{
   Q_OBJECT
public:
   explicit ListModel(QObject *parent = nullptr) : QAbstractListModel(parent) {}
   void setUsers(const std::vector<std::shared_ptr<Chat::UserData>> &users)
   {
      beginResetModel();
      users_.clear();
      for (const auto &user : users) {
         if (user) {
            users_.push_back(user);
         }
      }
      endResetModel();
   }
   int rowCount(const QModelIndex &parent = QModelIndex()) const override
   {
      Q_UNUSED(parent)
      return static_cast<int>(users_.size());
   }
   QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override
   {
      if (!index.isValid()) {
         return QVariant();
      }
      if (index.row() < 0 || index.row() >= static_cast<int>(users_.size())) {
         return QVariant();
      }
      switch (role) {
      case Qt::DisplayRole:
         return QVariant::fromValue(users_.at(static_cast<size_t>(index.row()))->getUserId());
      default:
         return QVariant();
      }
   }

private:
   std::vector<std::shared_ptr<Chat::UserData>> users_;
};

constexpr int kShowEmptyFoundUserListTimeoutMs = 3000;
constexpr int kMaxContentHeight = 130;

SearchWidget::SearchWidget(QWidget *parent)
   : QWidget(parent)
   , ui_(new Ui::SearchWidget)
   , listVisibleTimer_(new QTimer)
   , model_(new ListModel)
{
   ui_->setupUi(this);
   connect(ui_->chatSearchLineEdit, &ChatSearchLineEdit::textEdited,
           this, &SearchWidget::searchUserTextEdited);
   connect(ui_->chatSearchLineEdit, &ChatSearchLineEdit::textChanged,
           this, &SearchWidget::searchTextChanged);
}

SearchWidget::~SearchWidget()
{
}

void SearchWidget::init()
{
   setFixedHeight(kMaxContentHeight + ui_->chatSearchLineEdit->height());

   ui_->searchResultTreeView->setHeaderHidden(true);
   ui_->searchResultTreeView->setRootIsDecorated(false);
   ui_->searchResultTreeView->setFixedHeight(ui_->chatSearchLineEdit->height() * 3);
   ui_->searchResultTreeView->setVisible(false);

   listVisibleTimer_->setSingleShot(true);
   connect(listVisibleTimer_.get(), &QTimer::timeout, [this] {
      setListVisible(false);
   });
   ui_->searchResultTreeView->setModel(model_.get());
}

bool SearchWidget::isLineEditEnabled() const
{
   return ui_->chatSearchLineEdit->isEnabled();
}

bool SearchWidget::isListVisible() const
{
   return ui_->searchResultTreeView->isVisible();
}

QString SearchWidget::searchText() const
{
   return ui_->chatSearchLineEdit->text();
}

void SearchWidget::setUsers(const std::vector<std::shared_ptr<Chat::UserData> > &users)
{
   model_->setUsers(users);
}

void SearchWidget::clearLineEdit()
{
   ui_->chatSearchLineEdit->clear();
}

void SearchWidget::startListAutoHide()
{
   listVisibleTimer_->start(kShowEmptyFoundUserListTimeoutMs);
}

void SearchWidget::setLineEditEnabled(bool value)
{
   ui_->chatSearchLineEdit->setEnabled(value);
}

void SearchWidget::setListVisible(bool value)
{
   ui_->searchResultTreeView->setVisible(value);
   if (value) {
      ui_->chatUsersVerticalSpacer_->changeSize(
               20, kMaxContentHeight - ui_->chatSearchLineEdit->height() * 3);
   } else {
      ui_->chatUsersVerticalSpacer_->changeSize(20, kMaxContentHeight);
   }
   layout()->update();
   listVisibleTimer_->stop();
}

void SearchWidget::setSearchText(QString value)
{
   ui_->chatSearchLineEdit->setText(value);
}

#include "SearchWidget.moc"
