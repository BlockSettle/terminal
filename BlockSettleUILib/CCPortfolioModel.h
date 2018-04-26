#ifndef __CC_PORTFOLIO_MODEL__
#define __CC_PORTFOLIO_MODEL__

#include <QStandardItemModel>

#include <string>
#include <vector>
#include <memory>
#include <QFont>

class WalletsManager;
class AssetManager;

class CCPortfolioModel : public QStandardItemModel
{
Q_OBJECT
public:
   CCPortfolioModel(std::shared_ptr<WalletsManager> walletsManager, std::shared_ptr<AssetManager> assetManager, QObject *parent = nullptr);
   ~CCPortfolioModel() noexcept = default;

public:
   std::shared_ptr<AssetManager> assetManager();

private slots:
   void updateBlockchainData();

private:
   enum class CCPortfolioColumns : int
   {
      CCName,
      NetPos,
      LastPrice,
      NetValue,
      ColumnsCount
   };

   enum class TopLevelRows : int
   {
      Xbt,
      Cash,
      Shares
   };

   double xbtBalance_;
   std::shared_ptr<WalletsManager> walletsManager_;
   std::shared_ptr<AssetManager> assetManager_;
   QFont fontBold_;
   QList<QStandardItem*>   shareItems_;
   QList<QStandardItem*>   cashItems_;
   const QString           cashGroupName_;

private:
   void fillXbtWallets(QStandardItem *item);
   void updatePrivateShares();
   void updateCashAccountBalance(const std::string &currency);
   void updatePrivateShare(const std::string &cc, QList<QStandardItem*> &);
   void reloadSecurities();
   void updateCashTotalBalance();
};

#endif // __CC_PORTFOLIO_MODEL__
