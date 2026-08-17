// Copyright (c) 2011-2018 The XPChain Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XPCHAIN_QT_OVERVIEWPAGE_H
#define XPCHAIN_QT_OVERVIEWPAGE_H

#include <interfaces/wallet.h>
#include <QWidget>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QLabel>
#include <memory>

class ClientModel;
class TransactionFilterProxy;
class TxViewDelegate;
class PlatformStyle;
class WalletModel;
class AssetPieChart;
class TransactionAnalyticsWidget;

namespace Ui {
    class OverviewPage;
}

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

/** Overview ("home") page widget */
class OverviewPage : public QWidget
{
    Q_OBJECT

public:
    explicit OverviewPage(const PlatformStyle *platformStyle, QWidget *parent = 0);
    ~OverviewPage();

    void setClientModel(ClientModel *clientModel);
    void setWalletModel(WalletModel *walletModel);
    void showOutOfSyncWarning(bool fShow);

public Q_SLOTS:
    void setBalance(const interfaces::WalletBalances& balances);

Q_SIGNALS:
    void transactionClicked(const QModelIndex &index);
    void outOfSyncWarningClicked();

private:
    Ui::OverviewPage *ui;
    ClientModel *clientModel;
    WalletModel *walletModel;
    interfaces::WalletBalances m_balances;
    AssetPieChart *pieChart;
    TransactionAnalyticsWidget *analyticsWidget;

    // Phase 3 추가 멤버
    QNetworkAccessManager *networkManager;
    QTimer *apiTimer;
    QLabel *labelBadge;
    QLabel *labelStakingTimeCorrection;
    QWidget *walletStatusRow;
    QLabel *labelFormatChip;
    QLabel *labelFileChip;
    QLabel *labelTypeChip;
    QLabel *labelLockChip;

    // Phase 5 추가 멤버 (웹지갑 잔고 관찰)
    QNetworkAccessManager *watchNetworkManager;
    QTimer *watchTimer;
    double m_watchOnlyWebWalletBalance;

    TxViewDelegate *txdelegate;
    std::unique_ptr<TransactionFilterProxy> filter;

private Q_SLOTS:
    void updateDisplayUnit();
    void handleTransactionClicked(const QModelIndex &index);
    void updateAlerts(const QString &warnings);
    void updateWatchOnlyLabels(bool showWatchOnly);
    void handleOutOfSyncWarningClicks();

    // Phase 3 추가 슬롯
    void requestStakingData();
    void onStakingDataReceived(QNetworkReply* reply);
    void updateStakingTime(double networkWeight);
    void updateAssetBadge(double totalBalance);
    void updateWalletStatusChips();

    // Phase 5 추가 슬롯
    void onWatchAddressButtonClicked();
    void requestWatchAddressBalances();
    void onWatchAddressBalanceReceived(QNetworkReply* reply);
};

#endif // XPCHAIN_QT_OVERVIEWPAGE_H
