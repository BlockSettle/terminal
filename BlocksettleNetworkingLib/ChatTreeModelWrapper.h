#ifndef CHARTREEMODELWRAPPER_H
#define CHARTREEMODELWRAPPER_H

#include <QSortFilterProxyModel>

class ChatTreeModelWrapper : public QSortFilterProxyModel
{
   Q_OBJECT
public:
   explicit ChatTreeModelWrapper(QObject *parent = nullptr);

   /**
    * @reimp
    */
   void setSourceModel(QAbstractItemModel *sourceModel) override;

protected:
   /**
    * @reimp
    */
   bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;

private slots:
   /**
    * @brief Re-apply filtering and expand tree
    */
   void resetTree();

signals:
   void treeUpdated();

private:
   int filteringRole_;
   QVariant filteringValue_;
};

#endif // CHARTREEMODELWRAPPER_H
