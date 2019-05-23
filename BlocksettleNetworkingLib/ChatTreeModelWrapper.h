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
    * @param role Role of filtering property, -1 for ignore filtering
    */
   void setFilterKey(const QString &pattern, int role, bool caseSensitive = false);

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
   QString filteringPattern_;
   int filteringRole_;
   bool filteringCaseSensitive_;
};

#endif // CHARTREEMODELWRAPPER_H
