
#ifndef AUTHNOTICE_H_INCLUDED
#define AUTHNOTICE_H_INCLUDED

#include <QDialog>

#include <memory>


namespace Ui {
   class AuthNotice;
}


//
// AuthNotice
//

//! Just notice about Auth.
class AuthNotice : public QDialog
{
   Q_OBJECT

public:
   explicit AuthNotice(QWidget *parent);
   ~AuthNotice() override;

private:
   std::unique_ptr<Ui::AuthNotice> ui_;
}; // class AuthNotice

#endif // AUTHNOTICE_H_INCLUDED
