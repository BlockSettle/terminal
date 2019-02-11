#ifndef __MESSAGE_BOX_H__
#define __MESSAGE_BOX_H__

#include <QDialog>
#include <memory>

namespace Ui
{
   class BSMessageBox;
};

class BSMessageBox : public QDialog
{
Q_OBJECT

public:
   enum messageBoxType {
      info = 1,
      success = 2,
      question = 3,
      warning = 4,
      critical = 5
   };

   BSMessageBox(messageBoxType mbType
      , const QString& title, const QString& text
      , QWidget* parent = nullptr);

   BSMessageBox(messageBoxType mbType
      , const QString& title, const QString& text, const QString& details
      , QWidget* parent = nullptr);

   BSMessageBox(messageBoxType mbType
      , const QString& title, const QString& text
      , const QString& description, const QString& details
      , QWidget* parent = nullptr);

   ~BSMessageBox() override;
   void setConfirmButtonText(const QString &text);
   void setCancelButtonText(const QString &text);
   void setLabelTextFormat(Qt::TextFormat tf);

   void showEvent(QShowEvent *) override;
protected slots:
   void onDetailsPressed();

private:
   bool detailsVisible() const;
   void hideDetails();
   void showDetails();
   void setType(messageBoxType type);

private:
   std::unique_ptr<Ui::BSMessageBox> ui_;
};

class MessageBoxCCWalletQuestion : public BSMessageBox {
public:
   MessageBoxCCWalletQuestion(const QString &ccProduct, QWidget *parent = nullptr);
};

class MessageBoxBroadcastError : public BSMessageBox {
public:
   MessageBoxBroadcastError(const QString &details, QWidget *parent = nullptr);
};

class MessageBoxAuthNotice : public BSMessageBox {
public:
   MessageBoxAuthNotice(QWidget *parent = nullptr);
};

class MessageBoxWalletCreateAbort : public BSMessageBox {
public:
   MessageBoxWalletCreateAbort(QWidget *parent = nullptr);
};

#endif // __MESSAGE_BOX_H__
