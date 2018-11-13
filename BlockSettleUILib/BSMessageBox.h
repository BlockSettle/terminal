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
      mbInfo = 1,
      mbSuccess = 2,
      mbQuestion = 3,
      mbWarning = 4,
      mbCritical = 5
   };

   BSMessageBox(const QString& title, const QString& windowTitle
      , const QString& text, messageBoxType mbType = mbInfo
      , const QString& details = QString()
      , QWidget* parent = nullptr);

   ~BSMessageBox() override;

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


#endif // __MESSAGE_BOX_H__
