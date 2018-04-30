#include "FixedFeeValidator.h"

#include <QComboBox>
#include <QLineEdit>
#include <QTimer>


FixedFeeValidator::FixedFeeValidator(uint64_t initialFee, const QString& feeSuffix, QComboBox *parent)
   : QIntValidator(parent->lineEdit())
   , feeSuffix_(feeSuffix)
{
   setBottom(0);

   ConnectToComboBox(parent, initialFee);
}

void FixedFeeValidator::ConnectToComboBox(QComboBox *comboBox, uint64_t initialFee)
{
   comboBox->setValidator(this);
   lineEdit_ = comboBox->lineEdit();

   QString fullText = QString::number(initialFee) + feeSuffix_;

   lineEdit_->setText(fullText);
   lineEdit_->setCursorPosition(fullText.length() - feeSuffix_.length());

   //set cursor position
   connect(lineEdit_, &QLineEdit::cursorPositionChanged, this, &FixedFeeValidator::onCursorPositionChanged);
}

void FixedFeeValidator::onCursorPositionChanged(int oldPosition, int newPosition)
{
   auto maxPosition = lineEdit_->text().length() - feeSuffix_.length();
   if (newPosition > maxPosition) {
      lineEdit_->setCursorPosition(maxPosition);
   }
}

QIntValidator::State FixedFeeValidator::validate(QString &input, int &pos) const
{
   int feeValue = 0;
   State state = Invalid;

   if (input.isEmpty()) {
      state = Acceptable;
      input = feeSuffix_;
   }
   else {
      auto startPos = input.indexOf(feeSuffix_);
      if ((startPos == -1) || (startPos != input.size() - feeSuffix_.size())) {
         pos = 0;
         state = Invalid;
      }
      else {
         auto numberString = input.left(startPos);
         if (numberString.isEmpty()) {
            state = Intermediate;
         }
         else {
            state = QIntValidator::validate(numberString, pos);
            if (state != Invalid) {
               feeValue = numberString.toInt();
               if ((minValue_ > 0) && (feeValue < minValue_)) {
                  state = Intermediate;
               }
            }
         }
      }
   }

   if (state != Invalid) {
      emit feeUpdated(feeValue);
   }

   return state;
}

float FixedFeeValidator::getNumber(const QString &input) const
{
   const auto startPos = input.indexOf(feeSuffix_);
   if ((startPos == -1) || (startPos != input.size() - feeSuffix_.size())) {
      return -1;
   }
   return input.left(startPos).toFloat();
}

void FixedFeeValidator::fixup(QString &input) const
{
   if (input.isEmpty()) {
      return;
   }
   if ((minValue_ > 0) && (input.toFloat() < minValue_)) {
      input = QString::number(minValue_) + feeSuffix_;
      lineEdit_->setText(input);
   }
}
