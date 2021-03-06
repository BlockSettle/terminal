/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __MESSAGE_BOX_H__
#define __MESSAGE_BOX_H__

#include <QDialog>
#include <QVariant>
#include <memory>
#include "BSErrorCode.h"

namespace Ui
{
   class BSMessageBox;
};

class BSMessageBox : public QDialog
{
Q_OBJECT

public:
   enum Type {
      info = 1,
      success = 2,
      question = 3,
      warning = 4,
      critical = 5
   };

   static QString kUrlColor;

   BSMessageBox(Type mbType
      , const QString& title, const QString& text
      , QWidget* parent = nullptr);

   BSMessageBox(Type mbType
      , const QString& title, const QString& text, const QString& description
      , QWidget* parent = nullptr);

   BSMessageBox(Type mbType
      , const QString& title, const QString& text
      , const QString& description, const QString& details
      , QWidget* parent = nullptr);

   BSMessageBox(Type mbType
      , const QString& title, const QString& text
      , const QString& description, const QString& details
      , int forceWidth
      , QWidget* parent = nullptr);

   ~BSMessageBox() override;
   void setConfirmButtonText(const QString &text);
   void setCancelButtonText(const QString &text);
   void setLabelTextFormat(Qt::TextFormat tf);

   void showEvent(QShowEvent *) override;

   void setOkVisible(bool visible);
   void setCancelVisible(bool visible);
   void enableRichText();

protected slots:
   void onDetailsPressed();

protected:
   void setText(const QString &);

private:
   bool detailsVisible() const;
   void hideDetails();
   void showDetails();
   void setType(Type type);

private:
   std::unique_ptr<Ui::BSMessageBox> ui_;
};

class MessageBoxCCWalletQuestion : public BSMessageBox {
public:
   MessageBoxCCWalletQuestion(const QString &ccProduct, QWidget *parent = nullptr);
};

class MessageBoxBroadcastError : public BSMessageBox {
public:
   MessageBoxBroadcastError(const QString &details
      , bs::error::ErrorCode errCode, QWidget *parent = nullptr);
};

class MessageBoxExpTimeout : public BSMessageBox {
public:
   MessageBoxExpTimeout(QWidget *parent = nullptr);
};

#endif // __MESSAGE_BOX_H__
