// Copyright (c) 2011-2018 The XPChain Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/overviewpage.h>
#include <qt/forms/ui_overviewpage.h>
#include <qt/txanalytics.h>
#include <qt/watchaddressdialog.h>

#include <qt/xpchainunits.h>
#include <qt/clientmodel.h>
#include <qt/guiconstants.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>
#include <qt/platformstyle.h>
#include <qt/transactionfilterproxy.h>
#include <qt/transactiontablemodel.h>
#include <qt/walletmodel.h>

#include <util.h>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include <QAbstractItemDelegate>
#include <QPainter>
#include <QHBoxLayout>

#define DECORATION_SIZE 54
#define NUM_ITEMS 5

Q_DECLARE_METATYPE(interfaces::WalletBalances)

class TxViewDelegate : public QAbstractItemDelegate
{
    Q_OBJECT
public:
    explicit TxViewDelegate(const PlatformStyle *_platformStyle, QObject *parent=nullptr):
        QAbstractItemDelegate(parent), unit(XPChainUnits::XPC),
        platformStyle(_platformStyle)
    {

    }

    inline void paint(QPainter *painter, const QStyleOptionViewItem &option,
                      const QModelIndex &index ) const
    {
        painter->save();

        QIcon icon = qvariant_cast<QIcon>(index.data(TransactionTableModel::RawDecorationRole));
        QRect mainRect = option.rect;
        QRect decorationRect(mainRect.topLeft(), QSize(DECORATION_SIZE, DECORATION_SIZE));
        int xspace = DECORATION_SIZE + 8;
        int ypad = 6;
        int halfheight = (mainRect.height() - 2*ypad)/2;
        QRect amountRect(mainRect.left() + xspace, mainRect.top()+ypad, mainRect.width() - xspace, halfheight);
        QRect addressRect(mainRect.left() + xspace, mainRect.top()+ypad+halfheight, mainRect.width() - xspace, halfheight);
        icon = platformStyle->SingleColorIcon(icon);
        icon.paint(painter, decorationRect);

        QDateTime date = index.data(TransactionTableModel::DateRole).toDateTime();
        QString address = index.data(Qt::DisplayRole).toString();
        qint64 amount = index.data(TransactionTableModel::AmountRole).toLongLong();
        bool confirmed = index.data(TransactionTableModel::ConfirmedRole).toBool();
        QVariant value = index.data(Qt::ForegroundRole);
        QColor foreground = option.palette.color(QPalette::Text);
        if(value.canConvert<QBrush>())
        {
            QBrush brush = qvariant_cast<QBrush>(value);
            foreground = brush.color();
        }

        painter->setPen(foreground);
        QRect boundingRect;
        painter->drawText(addressRect, Qt::AlignLeft|Qt::AlignVCenter, address, &boundingRect);

        if (index.data(TransactionTableModel::WatchonlyRole).toBool())
        {
            QIcon iconWatchonly = qvariant_cast<QIcon>(index.data(TransactionTableModel::WatchonlyDecorationRole));
            QRect watchonlyRect(boundingRect.right() + 5, mainRect.top()+ypad+halfheight, 16, halfheight);
            iconWatchonly.paint(painter, watchonlyRect);
        }

        if(amount < 0)
        {
            foreground = COLOR_NEGATIVE;
        }
        else if(!confirmed)
        {
            foreground = COLOR_UNCONFIRMED;
        }
        else
        {
            foreground = option.palette.color(QPalette::Text);
        }
        painter->setPen(foreground);
        QString amountText = XPChainUnits::formatWithUnit(unit, amount, true, XPChainUnits::separatorAlways);
        if(!confirmed)
        {
            amountText = QString("[") + amountText + QString("]");
        }
        painter->drawText(amountRect, Qt::AlignRight|Qt::AlignVCenter, amountText);

        painter->setPen(option.palette.color(QPalette::Text));
        painter->drawText(amountRect, Qt::AlignLeft|Qt::AlignVCenter, GUIUtil::dateTimeStr(date));

        painter->restore();
    }

    inline QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        return QSize(DECORATION_SIZE, DECORATION_SIZE);
    }

    int unit;
    const PlatformStyle *platformStyle;

};
#include <qt/overviewpage.moc>

OverviewPage::OverviewPage(const PlatformStyle *platformStyle, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::OverviewPage),
    clientModel(0),
    walletModel(0),
    txdelegate(new TxViewDelegate(platformStyle, this)),
    networkManager(nullptr),
    apiTimer(nullptr),
    labelStakingTimeCorrection(nullptr),
    walletStatusRow(nullptr),
    labelFormatChip(nullptr),
    labelFileChip(nullptr),
    labelTypeChip(nullptr),
    labelLockChip(nullptr),
    watchNetworkManager(nullptr),
    watchTimer(nullptr),
    m_watchOnlyWebWalletBalance(0.0)
{
    ui->setupUi(this);

    m_balances.balance = -1;

    // Staking estimate — quiet secondary line under balances
    labelStakingTimeCorrection = new QLabel(this);
    labelStakingTimeCorrection->setWordWrap(true);
    labelStakingTimeCorrection->setStyleSheet(
        "QLabel { font-size: 11px; color: #8b949e; padding: 2px 2px 6px 2px; background: transparent; border: none; }");
    labelStakingTimeCorrection->setText(tr("Staking estimate: calculating…"));
    ui->verticalLayout_2->addWidget(labelStakingTimeCorrection);

    walletStatusRow = new QWidget(this);
    QHBoxLayout* walletStatusLayout = new QHBoxLayout(walletStatusRow);
    walletStatusLayout->setContentsMargins(0, 0, 0, 4);
    walletStatusLayout->setSpacing(6);
    auto makeChip = [this]() {
        QLabel* chip = new QLabel(this);
        chip->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        chip->setStyleSheet(
            "QLabel { font-size: 10px; font-weight: 600; color: #8b949e; "
            "background-color: transparent; border: 1px solid #3d444d; "
            "border-radius: 3px; padding: 2px 8px; }");
        return chip;
    };
    labelFormatChip = makeChip();
    labelFileChip = makeChip();
    labelTypeChip = makeChip();
    labelLockChip = makeChip();
    walletStatusLayout->addWidget(labelFormatChip);
    walletStatusLayout->addWidget(labelFileChip);
    walletStatusLayout->addWidget(labelTypeChip);
    walletStatusLayout->addWidget(labelLockChip);
    walletStatusLayout->addStretch();
    ui->verticalLayout_4->insertWidget(1, walletStatusRow);
    walletStatusRow->hide();

    // use a SingleColorIcon for the "out of sync warning" icon
    QIcon icon = platformStyle->SingleColorIcon(":/icons/warning");
    icon.addPixmap(icon.pixmap(QSize(64,64), QIcon::Normal), QIcon::Disabled); // also set the disabled icon because we are using a disabled QPushButton to work around missing HiDPI support of QLabel (https://bugreports.qt.io/browse/QTBUG-42503)
    ui->labelTransactionsStatus->setIcon(icon);
    ui->labelWalletStatus->setIcon(icon);

    // Recent transactions
    ui->listTransactions->setItemDelegate(txdelegate);
    ui->listTransactions->setIconSize(QSize(DECORATION_SIZE, DECORATION_SIZE));
    ui->listTransactions->setMinimumHeight(NUM_ITEMS * (DECORATION_SIZE + 2));
    ui->listTransactions->setAttribute(Qt::WA_MacShowFocusRect, false);

    connect(ui->listTransactions, SIGNAL(clicked(QModelIndex)), this, SLOT(handleTransactionClicked(QModelIndex)));

    // start with displaying the "out of sync" warnings
    showOutOfSyncWarning(true);
    connect(ui->labelWalletStatus, SIGNAL(clicked()), this, SLOT(handleOutOfSyncWarningClicks()));
    connect(ui->labelTransactionsStatus, SIGNAL(clicked()), this, SLOT(handleOutOfSyncWarningClicks()));

    // Phase 3: HTTP API 연동 타이머 구동
    networkManager = new QNetworkAccessManager(this);
    connect(networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(onStakingDataReceived(QNetworkReply*)));

    apiTimer = new QTimer(this);
    connect(apiTimer, SIGNAL(timeout()), this, SLOT(requestStakingData()));
    apiTimer->start(60000); // 60초 주기

    // Phase 5: 웹지갑 관리 버튼 추가
    QPushButton* watchAddressButton = new QPushButton(tr("Web Wallet Settings"), this);
    watchAddressButton->setStyleSheet(
        "QPushButton {"
        "  background-color: transparent;"
        "  color: #c9d1d9;"
        "  border: 1px solid #3d444d;"
        "  border-radius: 3px;"
        "  padding: 3px 10px;"
        "  font-size: 11px;"
        "  font-weight: 600;"
        "}"
        "QPushButton:hover {"
        "  background-color: #1f6feb22;"
        "  border: 1px solid #106ba3;"
        "  color: #ffffff;"
        "}"
        "QPushButton:pressed {"
        "  background-color: #0d4f7a;"
        "  color: #ffffff;"
        "}"
    );
    ui->horizontalLayout_4->addWidget(watchAddressButton);
    connect(watchAddressButton, SIGNAL(clicked()), this, SLOT(onWatchAddressButtonClicked()));

    // Phase 5: 웹지갑 잔고 스캔 타이머 및 네트워크 설정
    watchNetworkManager = new QNetworkAccessManager(this);
    connect(watchNetworkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(onWatchAddressBalanceReceived(QNetworkReply*)));

    watchTimer = new QTimer(this);
    connect(watchTimer, SIGNAL(timeout()), this, SLOT(requestWatchAddressBalances()));
    watchTimer->start(60000); // 60초 주기

    // 시작 시 즉시 웹지갑 잔고 조회 트리거
    QTimer::singleShot(2000, this, SLOT(requestWatchAddressBalances()));
}

void OverviewPage::handleTransactionClicked(const QModelIndex &index)
{
    if(filter && index.isValid())
        Q_EMIT transactionClicked(filter->mapToSource(index));
}

void OverviewPage::handleOutOfSyncWarningClicks()
{
    Q_EMIT outOfSyncWarningClicked();
}

OverviewPage::~OverviewPage()
{
    if (apiTimer) {
        apiTimer->stop();
    }
    if (networkManager) {
        networkManager->disconnect();
    }
    if (watchTimer) {
        watchTimer->stop();
    }
    if (watchNetworkManager) {
        watchNetworkManager->disconnect();
    }
    delete ui;
}

void OverviewPage::setBalance(const interfaces::WalletBalances& balances)
{
    if (!walletModel || !walletModel->getOptionsModel()) return;
    int unit = walletModel->getOptionsModel()->getDisplayUnit();
    m_balances = balances;
    ui->labelBalance->setText(XPChainUnits::formatWithUnit(unit, balances.balance, false, XPChainUnits::separatorAlways));
    ui->labelUnconfirmed->setText(XPChainUnits::formatWithUnit(unit, balances.unconfirmed_balance, false, XPChainUnits::separatorAlways));
    ui->labelImmature->setText(XPChainUnits::formatWithUnit(unit, balances.immature_balance, false, XPChainUnits::separatorAlways));

    qint64 walletTotal = balances.balance + balances.unconfirmed_balance + balances.immature_balance;
    ui->labelTotal->setText(XPChainUnits::formatWithUnit(unit, walletTotal, false, XPChainUnits::separatorAlways));
    ui->labelTotal->setToolTip(tr("Spendable wallet balance (does not include external web wallet watch addresses)"));

    qint64 watchOnlySatoshi = static_cast<qint64>(m_watchOnlyWebWalletBalance * 10000.0);
    qint64 coreWatchOnly = balances.watch_only_balance + balances.unconfirmed_watch_only_balance + balances.immature_watch_only_balance;

    ui->labelWatchAvailable->setText(XPChainUnits::formatWithUnit(unit, balances.watch_only_balance + watchOnlySatoshi, false, XPChainUnits::separatorAlways));
    ui->labelWatchAvailable->setToolTip(tr("Watch-only balances including linked web wallet addresses (not spendable from this node)"));
    ui->labelWatchPending->setText(XPChainUnits::formatWithUnit(unit, balances.unconfirmed_watch_only_balance, false, XPChainUnits::separatorAlways));
    ui->labelWatchImmature->setText(XPChainUnits::formatWithUnit(unit, balances.immature_watch_only_balance, false, XPChainUnits::separatorAlways));
    ui->labelWatchTotal->setText(XPChainUnits::formatWithUnit(unit, coreWatchOnly + watchOnlySatoshi, false, XPChainUnits::separatorAlways));
    ui->labelWatchTotal->setToolTip(tr("Combined watch-only total (core watch addresses + web wallet API balances)"));

    // only show immature (newly mined) balance if it's non-zero, so as not to complicate things
    // for the non-mining users
    bool showImmature = balances.immature_balance != 0;
    bool showWatchOnlyImmature = balances.immature_watch_only_balance != 0;

    // for symmetry reasons also show immature label when the watch-only one is shown
    ui->labelImmature->setVisible(showImmature || showWatchOnlyImmature);
    ui->labelImmatureText->setVisible(showImmature || showWatchOnlyImmature);
    ui->labelWatchImmature->setVisible(showWatchOnlyImmature); // show watch-only immature balance
}

// show/hide watch-only labels
void OverviewPage::updateWatchOnlyLabels(bool showWatchOnly)
{
    bool hasWatchAddress = !TxAnalytics::getInstance().getWatchAddresses().isEmpty();
    bool finalShow = showWatchOnly || hasWatchAddress;

    ui->labelSpendable->setVisible(finalShow);      // show spendable label (only when watch-only is active)
    ui->labelWatchonly->setVisible(finalShow);      // show watch-only label
    ui->lineWatchBalance->setVisible(finalShow);    // show watch-only balance separator line
    ui->labelWatchAvailable->setVisible(finalShow); // show watch-only available balance
    ui->labelWatchPending->setVisible(finalShow);   // show watch-only pending balance
    ui->labelWatchTotal->setVisible(finalShow);     // show watch-only total balance

    if (!finalShow)
        ui->labelWatchImmature->hide();
}

void OverviewPage::setClientModel(ClientModel *model)
{
    this->clientModel = model;
    if(model)
    {
        // Show warning if this is a prerelease version
        connect(model, SIGNAL(alertsChanged(QString)), this, SLOT(updateAlerts(QString)));
        updateAlerts(model->getStatusBarWarnings());
    }
}

void OverviewPage::setWalletModel(WalletModel *model)
{
    this->walletModel = model;
    if(model && model->getOptionsModel())
    {
        // Set up transaction list
        filter.reset(new TransactionFilterProxy());
        filter->setSourceModel(model->getTransactionTableModel());
        filter->setLimit(NUM_ITEMS);
        filter->setDynamicSortFilter(true);
        filter->setSortRole(Qt::EditRole);
        filter->setShowInactive(false);
        filter->sort(TransactionTableModel::Date, Qt::DescendingOrder);

        ui->listTransactions->setModel(filter.get());
        ui->listTransactions->setModelColumn(TransactionTableModel::ToAddress);

        // Keep up to date with wallet
        interfaces::Wallet& wallet = model->wallet();
        interfaces::WalletBalances balances = wallet.getBalances();
        setBalance(balances);
        connect(model, SIGNAL(balanceChanged(interfaces::WalletBalances)), this, SLOT(setBalance(interfaces::WalletBalances)));

        connect(model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));

        updateWatchOnlyLabels(wallet.haveWatchOnly());
        connect(model, SIGNAL(notifyWatchonlyChanged(bool)), this, SLOT(updateWatchOnlyLabels(bool)));
        connect(model, SIGNAL(encryptionStatusChanged()), this, SLOT(updateWalletStatusChips()));
        updateWalletStatusChips();

        // Phase 3: TxAnalytics (tags / web wallet) — ensure DB is open
        QString dataDir = QString::fromStdString(GetDataDir().string());
        TxAnalytics::getInstance().init(dataDir);

        // Phase 3: 최초 즉각 1회 API 연동 실행
        requestStakingData();
    } else {
        updateWalletStatusChips();
    }

    // update the display unit, to not use the default ("BTC")
    updateDisplayUnit();
}

void OverviewPage::updateDisplayUnit()
{
    if(walletModel && walletModel->getOptionsModel())
    {
        if (m_balances.balance != -1) {
            setBalance(m_balances);
        }

        // Update txdelegate->unit with the current unit
        txdelegate->unit = walletModel->getOptionsModel()->getDisplayUnit();

        ui->listTransactions->update();
    }
}

void OverviewPage::updateAlerts(const QString &warnings)
{
    this->ui->labelAlerts->setVisible(!warnings.isEmpty());
    this->ui->labelAlerts->setText(warnings);
}

void OverviewPage::showOutOfSyncWarning(bool fShow)
{
    ui->labelWalletStatus->setVisible(fShow);
    ui->labelTransactionsStatus->setVisible(fShow);
}

void OverviewPage::requestStakingData()
{
    if (!networkManager) return;
    QNetworkRequest request(QUrl("https://explorer.xpchain.co.kr/ext/summary"));
    networkManager->get(request);
}

void OverviewPage::onStakingDataReceived(QNetworkReply* reply)
{
    if (!reply) return;

    double networkWeight = 10000000.0; // 기본값

    if (reply->error() == QNetworkReply::NoError) {
        QByteArray responseData = reply->readAll();
        QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
        if (!jsonDoc.isNull() && jsonDoc.isObject()) {
            QJsonObject jsonObj = jsonDoc.object();
            if (jsonObj.contains("data") && jsonObj["data"].isArray()) {
                QJsonArray dataArray = jsonObj["data"].toArray();
                if (!dataArray.isEmpty() && dataArray.at(0).isObject()) {
                    QJsonObject dataObj = dataArray.at(0).toObject();
                    if (dataObj.contains("stakingw")) {
                        networkWeight = dataObj["stakingw"].toDouble();
                    } else if (dataObj.contains("difficulty")) {
                        networkWeight = dataObj["difficulty"].toDouble() * 16700000.0;
                    }
                }
            } else {
                if (jsonObj.contains("stakingw")) {
                    networkWeight = jsonObj["stakingw"].toDouble();
                } else if (jsonObj.contains("difficulty")) {
                    networkWeight = jsonObj["difficulty"].toDouble() * 16700000.0;
                }
            }
        }
    } else {
        qWarning() << "Explorer API error:" << reply->errorString();
    }

    updateStakingTime(networkWeight);
    reply->deleteLater();
}

void OverviewPage::updateStakingTime(double networkWeight)
{
    if (!walletModel || !labelStakingTimeCorrection) return;

    double myWeight = (m_balances.balance + m_balances.immature_balance) / 10000.0;

    if (myWeight <= 0.0) {
        labelStakingTimeCorrection->setText(tr("Staking estimate: wallet balance required."));
        return;
    }

    if (networkWeight <= 0.0) {
        networkWeight = 10000000.0;
    }

    double expectedTimeMinutes = (networkWeight / myWeight) * 1.0;

    QString timeText;
    if (expectedTimeMinutes < 60.0) {
        timeText = QString(tr("Staking estimate: ~%1 min (indicative only)")).arg(expectedTimeMinutes, 0, 'f', 1);
    } else if (expectedTimeMinutes < 1440.0) {
        double hours = expectedTimeMinutes / 60.0;
        timeText = QString(tr("Staking estimate: ~%1 hours (indicative only)")).arg(hours, 0, 'f', 1);
    } else {
        double days = expectedTimeMinutes / 1440.0;
        timeText = QString(tr("Staking estimate: ~%1 days (indicative only)")).arg(days, 0, 'f', 1);
    }

    timeText += tr(" — coins must mature ~3 days before staking");
    labelStakingTimeCorrection->setText(timeText);
}

namespace {
void styleWalletChip(QLabel* label, const QString& text, const QString& color, const QString& border, const QString& tip)
{
    if (!label) return;
    label->setText(text);
    label->setToolTip(tip);
    label->setVisible(!text.isEmpty());
    label->setStyleSheet(QStringLiteral(
        "QLabel { font-size: 10px; font-weight: 600; color: %1; "
        "background-color: transparent; border: 1px solid %2; "
        "border-radius: 3px; padding: 2px 8px; }").arg(color, border));
}
} // namespace

void OverviewPage::updateWalletStatusChips()
{
    if (!walletStatusRow) return;
    if (!walletModel) {
        walletStatusRow->hide();
        return;
    }

    interfaces::Wallet& wallet = walletModel->wallet();
    const QString format = QString::fromStdString(wallet.databaseFormat());
    const bool sqlite = (format == QLatin1String("sqlite"));
    const bool at_rest = wallet.isEncryptedAtRest();
    const bool descriptor = wallet.isDescriptor();
    const bool watch_only = walletModel->privateKeysDisabled();
    const WalletModel::EncryptionStatus enc = walletModel->getEncryptionStatus();

    if (sqlite) {
        styleWalletChip(labelFormatChip, tr("SQLite"), QStringLiteral("#6cb6ff"), QStringLiteral("#1f6feb"),
                        tr("Wallet database engine: SQLite (recommended)."));
        if (at_rest) {
            styleWalletChip(labelFileChip, tr("SQLCipher"), QStringLiteral("#2e7d32"), QStringLiteral("#2e7d32"),
                            tr("Wallet file is encrypted at rest (SQLCipher). Opening the file and unlocking spending keys are separate steps."));
        } else {
            styleWalletChip(labelFileChip, tr("Plain file"), QStringLiteral("#8b949e"), QStringLiteral("#3d444d"),
                            tr("Wallet file is not SQLCipher-encrypted. Encrypt Wallet also encrypts the SQLite file on SQLCipher builds."));
        }
    } else {
        styleWalletChip(labelFormatChip, tr("Berkeley DB"), QStringLiteral("#ef6c00"), QStringLiteral("#ef6c00"),
                        tr("Legacy Berkeley DB wallet. Use File → Migrate Wallet to SQLite… to convert (original file is kept)."));
        styleWalletChip(labelFileChip, tr("BDB file"), QStringLiteral("#8b949e"), QStringLiteral("#3d444d"),
                        tr("Berkeley DB storage. SQLCipher at-rest encryption applies to SQLite wallets only."));
    }

    if (watch_only) {
        styleWalletChip(labelTypeChip, tr("Watch-only"), QStringLiteral("#8b949e"), QStringLiteral("#3d444d"),
                        tr("Private keys are disabled. This wallet cannot spend."));
    } else if (descriptor) {
        styleWalletChip(labelTypeChip, tr("Descriptor"), QStringLiteral("#6cb6ff"), QStringLiteral("#1f6feb"),
                        tr("Descriptor wallet (modern key management)."));
    } else {
        styleWalletChip(labelTypeChip, tr("Legacy HD"), QStringLiteral("#8b949e"), QStringLiteral("#3d444d"),
                        tr("Legacy HD wallet. Descriptor wallets are recommended for new funds."));
    }

    if (watch_only) {
        styleWalletChip(labelLockChip, tr("No keys"), QStringLiteral("#8b949e"), QStringLiteral("#3d444d"),
                        tr("Watch-only wallet: no spending keys to lock or unlock."));
    } else if (enc == WalletModel::Unencrypted) {
        styleWalletChip(labelLockChip, tr("Unencrypted"), QStringLiteral("#ef6c00"), QStringLiteral("#ef6c00"),
                        tr("Spending keys are not encrypted. Use Settings → Encrypt Wallet."));
    } else if (enc == WalletModel::Locked) {
        styleWalletChip(labelLockChip, tr("Keys locked"), QStringLiteral("#ef6c00"), QStringLiteral("#ef6c00"),
                        tr("Wallet file may already be open, but spending keys are locked. Unlock before sending or signing."));
    } else if (fWalletUnlockMintOnly) {
        styleWalletChip(labelLockChip, tr("Staking only"), QStringLiteral("#f0c14b"), QStringLiteral("#8a6d1d"),
                        tr("Spending keys are unlocked for staking only. Sending still requires a full unlock."));
    } else {
        styleWalletChip(labelLockChip, tr("Keys unlocked"), QStringLiteral("#2e7d32"), QStringLiteral("#2e7d32"),
                        tr("Spending keys are unlocked (sending and signing allowed)."));
    }

    walletStatusRow->show();
}

void OverviewPage::onWatchAddressButtonClicked()
{
    WatchAddressDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        if (walletModel && walletModel->getOptionsModel()) {
            updateWatchOnlyLabels(walletModel->wallet().haveWatchOnly());
        }
        requestWatchAddressBalances();
    }
}

void OverviewPage::requestWatchAddressBalances()
{
    if (!watchNetworkManager) return;

    QList<TxAnalytics::WatchAddress> list = TxAnalytics::getInstance().getWatchAddresses();
    if (list.isEmpty()) {
        m_watchOnlyWebWalletBalance = 0.0;
        if (walletModel && walletModel->getOptionsModel()) {
            updateWatchOnlyLabels(walletModel->wallet().haveWatchOnly());
        }
        setBalance(m_balances);
        return;
    }

    for (const auto& wa : list) {
        QUrl url(QString("https://explorer.xpchain.co.kr/ext/getbalance/%1").arg(wa.address));
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::UserAgentHeader, "XPChainCore-HybridDashboard/1.0");

        QNetworkReply* reply = watchNetworkManager->get(request);
        reply->setProperty("address", wa.address);
    }
}

void OverviewPage::onWatchAddressBalanceReceived(QNetworkReply* reply)
{
    if (!reply) return;

    QString address = reply->property("address").toString();
    if (address.isEmpty()) {
        reply->deleteLater();
        return;
    }

    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        QString responseStr = QString::fromUtf8(data).trimmed();
        bool ok = false;
        double balance = 0.0;

        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isNull() && doc.isObject()) {
            QJsonObject obj = doc.object();
            if (obj.contains("balance")) {
                balance = obj.value("balance").toDouble();
                ok = true;
            }
        }

        if (!ok) {
            balance = responseStr.toDouble(&ok);
        }

        if (ok) {
            TxAnalytics::getInstance().updateWatchAddressBalance(address, balance);
            m_watchOnlyWebWalletBalance = TxAnalytics::getInstance().getWatchAddressesTotalBalance();
            if (walletModel && walletModel->getOptionsModel()) {
                updateWatchOnlyLabels(walletModel->wallet().haveWatchOnly());
                setBalance(m_balances);
            }
        } else {
            qWarning() << "Failed to parse balance from explorer API for address:" << address << "Response:" << responseStr;
            m_watchOnlyWebWalletBalance = TxAnalytics::getInstance().getWatchAddressesTotalBalance();
            if (walletModel && walletModel->getOptionsModel()) {
                updateWatchOnlyLabels(walletModel->wallet().haveWatchOnly());
                setBalance(m_balances);
            }
        }
    } else {
        qWarning() << "Explorer API network error for address:" << address << ":" << reply->errorString();
        m_watchOnlyWebWalletBalance = TxAnalytics::getInstance().getWatchAddressesTotalBalance();
        if (walletModel && walletModel->getOptionsModel()) {
            updateWatchOnlyLabels(walletModel->wallet().haveWatchOnly());
            setBalance(m_balances);
        }
    }

    reply->deleteLater();
}
