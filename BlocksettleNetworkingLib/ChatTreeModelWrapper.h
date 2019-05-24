#ifndef CHARTREEMODELWRAPPER_H
#define CHARTREEMODELWRAPPER_H

#include <QSortFilterProxyModel>

class ChatTreeModelWrapper : public QSortFilterProxyModel
{
   Q_OBJECT
public:
   explicit ChatTreeModelWrapper(QObject *parent = nullptr);

   /**
    * @brief Filter chat rooms by key applied to specified role
    * @param pattern String contained by branch property
    * @param role Filtering role. Should be ChatClientDataModel::Role. Qt::Display by default
    * @param caseSensitive Use case sensitive filtering or not. False by default
    */
   void setFilterKey(const QString &pattern,
                     int role = Qt::DisplayRole,
                     bool caseSensitive = false);

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
};

#endif // CHARTREEMODELWRAPPER_H
