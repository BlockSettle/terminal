
#ifndef FREJANOTICE_H_INCLUDED
#define FREJANOTICE_H_INCLUDED

#include <QDialog>

#include <memory>


namespace Ui {
   class FrejaNotice;
}


//
// FrejaNotice
//

//! Just notice about Freja.
class FrejaNotice : public QDialog
{
   Q_OBJECT

public:
   explicit FrejaNotice(QWidget *parent);
   ~FrejaNotice() override;

private:
   std::unique_ptr<Ui::FrejaNotice> ui_;
}; // class FrejaNotice

#endif // FREJANOTICE_H_INCLUDED
