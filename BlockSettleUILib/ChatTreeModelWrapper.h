#ifndef CHARTREEMODELWRAPPER_H
#define CHARTREEMODELWRAPPER_H

#include <QIdentityProxyModel>

class ChatTreeModelWrapper : public QIdentityProxyModel
{
   Q_OBJECT
public:
   explicit ChatTreeModelWrapper(QObject *parent = nullptr);

   /**
    * @reimp
    */
   virtual bool hasChildren(const QModelIndex &parent = QModelIndex()) const override;

private:
   /**
    * @brief Check if node should expand and contain child nodes
    * @param parent
    * @return
    */
   bool childrenVisible(const QModelIndex &parent = QModelIndex()) const;
};

#endif // CHARTREEMODELWRAPPER_H
