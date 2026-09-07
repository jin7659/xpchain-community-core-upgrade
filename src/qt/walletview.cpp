// Copyright (c) 2011-2018 The XPChain Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/walletview.h>

#include <qt/addressbookpage.h>
#include <qt/askpassphrasedialog.h>
#include <qt/xpchaingui.h>
#include <qt/clientmodel.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>
#include <qt/overviewpage.h>
#include <qt/platformstyle.h>
#include <qt/receivecoinsdialog.h>
#include <qt/sendcoinsdialog.h>
#include <qt/signverifymessagedialog.h>
#include <qt/stakingrewardsettingpage.h>
#include <qt/transactiontablemodel.h>
#include <qt/transactionview.h>
#include <qt/mintingtablemodel.h>
#include <qt/mintingview.h>
#include <qt/walletmodel.h>

#include <interfaces/node.h>
#include <ui_interface.h>

#include <QAction>
#include <QActionGroup>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QProgressDialog>
#include <QPushButton>
#include <QVBoxLayout>
#include <QThreadPool>
#include <QRunnable>
#include <QApplication>

WalletView::WalletView(const PlatformStyle *_platformStyle, QWidget *parent):
    QStackedWidget(parent),
    clientModel(0),
    walletModel(0),
    platformStyle(_platformStyle)
{
    // Create tabs
    overviewPage = new OverviewPage(platformStyle);

    transactionsPage = new QWidget(this);
    QVBoxLayout *vbox = new QVBoxLayout();
    QHBoxLayout *hbox_buttons = new QHBoxLayout();
    transactionView = new TransactionView(platformStyle, this);
    vbox->addWidget(transactionView);
    QPushButton *exportButton = new QPushButton(tr("&Export"), this);
    exportButton->setToolTip(tr("Export the data in the current tab to a file"));
    if (platformStyle->getImagesOnButtons()) {
        exportButton->setIcon(platformStyle->SingleColorIcon(":/icons/export"));
    }
    hbox_buttons->addStretch();
    hbox_buttons->addWidget(exportButton);
    vbox->addLayout(hbox_buttons);
    transactionsPage->setLayout(vbox);

    receiveCoinsPage = new ReceiveCoinsDialog(platformStyle);
    sendCoinsPage = new SendCoinsDialog(platformStyle);

    mintingPage = new QWidget(this);
    QVBoxLayout *vboxMinting = new QVBoxLayout();
    mintingView = new MintingView(platformStyle, this);
    connect(mintingView, &MintingView::unlockForStakingRequested, this, [this]() {
        decryptForMinting(true);
    });
    vboxMinting->addWidget(mintingView);
    mintingPage->setLayout(vboxMinting);

    usedSendingAddressesPage = new AddressBookPage(platformStyle, AddressBookPage::ForEditing, AddressBookPage::SendingTab, this);
    usedReceivingAddressesPage = new AddressBookPage(platformStyle, AddressBookPage::ForEditing, AddressBookPage::ReceivingTab, this);

    addWidget(overviewPage);
    addWidget(transactionsPage);
    addWidget(receiveCoinsPage);
    addWidget(sendCoinsPage);
    addWidget(mintingPage);

    // Clicking on a transaction on the overview pre-selects the transaction on the transaction history page
    connect(overviewPage, SIGNAL(transactionClicked(QModelIndex)), transactionView, SLOT(focusTransaction(QModelIndex)));
    connect(overviewPage, SIGNAL(outOfSyncWarningClicked()), this, SLOT(requestedSyncWarningInfo()));

    // Highlight transaction after send
    connect(sendCoinsPage, SIGNAL(coinsSent(uint256)), transactionView, SLOT(focusTransaction(uint256)));

    // Double-clicking on a transaction on the transaction history page shows details
    connect(transactionView, SIGNAL(doubleClicked(QModelIndex)), transactionView, SLOT(showDetails()));

    // Clicking on "Export" allows to export the transaction list
    connect(exportButton, SIGNAL(clicked()), transactionView, SLOT(exportClicked()));

    // Pass through messages from sendCoinsPage
    connect(sendCoinsPage, SIGNAL(message(QString,QString,unsigned int)), this, SIGNAL(message(QString,QString,unsigned int)));
    // Pass through messages from transactionView
    connect(transactionView, SIGNAL(message(QString,QString,unsigned int)), this, SIGNAL(message(QString,QString,unsigned int)));
}

WalletView::~WalletView()
{
}

void WalletView::setXPChainGUI(XPChainGUI *gui)
{
    if (gui)
    {
        // Clicking on a transaction on the overview page simply sends you to transaction history page
        connect(overviewPage, SIGNAL(transactionClicked(QModelIndex)), gui, SLOT(gotoHistoryPage()));

        // Clicking migrate on the overview BDB banner opens the migration dialog
        connect(overviewPage, &OverviewPage::migrateWalletClicked, gui, &XPChainGUI::migrateWallet);

        // Navigate to transaction history page after send
        connect(sendCoinsPage, SIGNAL(coinsSent(uint256)), gui, SLOT(gotoHistoryPage()));

        // Receive and report messages
        connect(this, SIGNAL(message(QString,QString,unsigned int)), gui, SLOT(message(QString,QString,unsigned int)));

        // Pass through encryption status changed signals
        connect(this, SIGNAL(encryptionStatusChanged()), gui, SLOT(updateWalletStatus()));

        // Pass through transaction notifications
        connect(this, SIGNAL(incomingTransaction(QString,int,CAmount,QString,QString,QString,QString)), gui, SLOT(incomingTransaction(QString,int,CAmount,QString,QString,QString,QString)));

        // Connect HD enabled state signal
        connect(this, SIGNAL(hdEnabledStatusChanged()), gui, SLOT(updateWalletStatus()));
    }
}

void WalletView::setClientModel(ClientModel *_clientModel)
{
    this->clientModel = _clientModel;

    overviewPage->setClientModel(_clientModel);
    sendCoinsPage->setClientModel(_clientModel);
}

void WalletView::setWalletModel(WalletModel *_walletModel)
{
    this->walletModel = _walletModel;

    // Put transaction list in tabs
    transactionView->setModel(_walletModel);
    mintingView->setModel(_walletModel);
    overviewPage->setWalletModel(_walletModel);
    receiveCoinsPage->setModel(_walletModel);
    sendCoinsPage->setModel(_walletModel);
    usedReceivingAddressesPage->setModel(_walletModel ? _walletModel->getAddressTableModel() : nullptr);
    usedSendingAddressesPage->setModel(_walletModel ? _walletModel->getAddressTableModel() : nullptr);

    if (_walletModel)
    {
        // Receive and pass through messages from wallet model
        connect(_walletModel, SIGNAL(message(QString,QString,unsigned int)), this, SIGNAL(message(QString,QString,unsigned int)));

        // Handle changes in encryption status
        connect(_walletModel, SIGNAL(encryptionStatusChanged()), this, SIGNAL(encryptionStatusChanged()));
        updateEncryptionStatus();

        // update HD status
        Q_EMIT hdEnabledStatusChanged();

        // Balloon pop-up for new transaction
        connect(_walletModel->getTransactionTableModel(), SIGNAL(rowsInserted(QModelIndex,int,int)),
                this, SLOT(processNewTransaction(QModelIndex,int,int)));

        // Ask for passphrase if needed
        connect(_walletModel, SIGNAL(requireUnlock()), this, SLOT(unlockWallet()));

        // Show progress dialog
        connect(_walletModel, SIGNAL(showProgress(QString,int)), this, SLOT(showProgress(QString,int)));
    }
}

void WalletView::processNewTransaction(const QModelIndex& parent, int start, int /*end*/)
{
    // Prevent balloon-spam when initial block download is in progress
    if (!walletModel || !clientModel || clientModel->node().isInitialBlockDownload())
        return;

    TransactionTableModel *ttm = walletModel->getTransactionTableModel();
    if (!ttm || ttm->processingQueuedTransactions())
        return;

    QString date = ttm->index(start, TransactionTableModel::Date, parent).data().toString();
    qint64 amount = ttm->index(start, TransactionTableModel::Amount, parent).data(Qt::EditRole).toULongLong();
    QString type = ttm->index(start, TransactionTableModel::Type, parent).data().toString();
    QModelIndex index = ttm->index(start, 0, parent);
    QString address = ttm->data(index, TransactionTableModel::AddressRole).toString();
    QString label = ttm->data(index, TransactionTableModel::LabelRole).toString();

    Q_EMIT incomingTransaction(date, walletModel->getOptionsModel()->getDisplayUnit(), amount, type, address, label, walletModel->getWalletName());
}

void WalletView::fadeToWidget(QWidget *widget)
{
    if (!widget || currentWidget() == widget)
        return;

    // Clear focus before switching tabs so QAbstractItemView::focusInEvent does not
    // query proxy model flags while the stacked page is still being activated.
    if (QWidget *focusWidget = QApplication::focusWidget()) {
        if (focusWidget == this || isAncestorOf(focusWidget))
            focusWidget->clearFocus();
    }

    setCurrentWidget(widget);
}

void WalletView::gotoOverviewPage()
{
    fadeToWidget(overviewPage);
}

void WalletView::gotoHistoryPage()
{
    fadeToWidget(transactionsPage);
}

void WalletView::gotoReceiveCoinsPage()
{
    fadeToWidget(receiveCoinsPage);
}

void WalletView::gotoSendCoinsPage(QString addr)
{
    fadeToWidget(sendCoinsPage);

    if (!addr.isEmpty())
        sendCoinsPage->setAddress(addr);
}

void WalletView::gotoMintingPage()
{
    fadeToWidget(mintingPage);
}

void WalletView::gotoSignMessageTab(QString addr)
{
    // calls show() in showTab_SM()
    SignVerifyMessageDialog *signVerifyMessageDialog = new SignVerifyMessageDialog(platformStyle, this);
    signVerifyMessageDialog->setAttribute(Qt::WA_DeleteOnClose);
    signVerifyMessageDialog->setModel(walletModel);
    signVerifyMessageDialog->showTab_SM(true);

    if (!addr.isEmpty())
        signVerifyMessageDialog->setAddress_SM(addr);
}

void WalletView::gotoVerifyMessageTab(QString addr)
{
    // calls show() in showTab_VM()
    SignVerifyMessageDialog *signVerifyMessageDialog = new SignVerifyMessageDialog(platformStyle, this);
    signVerifyMessageDialog->setAttribute(Qt::WA_DeleteOnClose);
    signVerifyMessageDialog->setModel(walletModel);
    signVerifyMessageDialog->showTab_VM(true);

    if (!addr.isEmpty())
        signVerifyMessageDialog->setAddress_VM(addr);
}

bool WalletView::handlePaymentRequest(const SendCoinsRecipient& recipient)
{
    return sendCoinsPage->handlePaymentRequest(recipient);
}

void WalletView::showOutOfSyncWarning(bool fShow)
{
    overviewPage->showOutOfSyncWarning(fShow);
}

void WalletView::updateEncryptionStatus()
{
    Q_EMIT encryptionStatusChanged();
}

void WalletView::encryptWallet(bool status)
{
    if(!walletModel)
        return;
    AskPassphraseDialog dlg(status ? AskPassphraseDialog::Encrypt : AskPassphraseDialog::Decrypt, this);
    dlg.setModel(walletModel);
    dlg.exec();

    updateEncryptionStatus();
}
void WalletView::decryptForMinting(bool status)
{
    if(!walletModel)
        return;

    if (status)
    {
        if(walletModel->getEncryptionStatus() != WalletModel::Locked)
            return;

        fWalletUnlockMintOnly = true;
        AskPassphraseDialog dlg(AskPassphraseDialog::Unlock, this,
            tr("Enter your wallet passphrase to <b>unlock spending keys for staking only</b>.<br/><br/>"
               "Sending coins will still require a full unlock. "
               "This is separate from opening an encrypted wallet file at startup."));
        dlg.setModel(walletModel);
        dlg.exec();

        if(walletModel->getEncryptionStatus() != WalletModel::Unlocked){
            fWalletUnlockMintOnly = false;
            updateEncryptionStatus();
            return;
         }
        updateEncryptionStatus();
    }
    else
    {
        if(walletModel->getEncryptionStatus() != WalletModel::Unlocked)
            return;

        if (!fWalletUnlockMintOnly)
            return;

        walletModel->setWalletLocked(true);
        fWalletUnlockMintOnly = false;
        updateEncryptionStatus();
    }
}

class BackupWorker : public QRunnable
{
public:
    BackupWorker(WalletModel* model, const std::string& filepath, QObject* receiver)
        : m_model(model), m_filepath(filepath), m_receiver(receiver) {}

    void run() override
    {
        bool success = m_model->wallet().backupWallet(m_filepath);
        QMetaObject::invokeMethod(m_receiver, "backupFinished",
                                  Qt::QueuedConnection,
                                  Q_ARG(bool, success),
                                  Q_ARG(QString, QString::fromStdString(m_filepath)));
    }

private:
    WalletModel* m_model;
    std::string m_filepath;
    QObject* m_receiver;
};

void WalletView::backupWallet()
{
    if (!walletModel) return;

    QString filter = tr("Wallet Data (*.dat)");
    QString suffix = "dat";
    if (walletModel->wallet().databaseFormat() == "sqlite") {
        filter = tr("SQLite Wallet (*.sqlite);;Wallet Data (*.dat)");
        suffix = "sqlite";
    }
    QString filename = GUIUtil::getSaveFileName(this,
        tr("Backup Wallet"), QString(),
        filter, nullptr);

    if (filename.isEmpty())
        return;

    // 비동기 백업 개시 정보성 발룬 알림
    Q_EMIT message(tr("Backup Wallet"), tr("Wallet backup has started in the background..."), CClientUIInterface::MSG_INFORMATION);

    // QThreadPool을 통해 논블로킹 백그라운드 태스크로 백업 수행
    BackupWorker* worker = new BackupWorker(walletModel, filename.toLocal8Bit().data(), this);
    worker->setAutoDelete(true);
    QThreadPool::globalInstance()->start(worker);
}

void WalletView::backupFinished(bool success, const QString &filename)
{
    if (!success) {
        Q_EMIT message(tr("Backup Failed"), tr("There was an error trying to save the wallet data to %1.").arg(filename),
            CClientUIInterface::MSG_ERROR);
    } else {
        Q_EMIT message(tr("Backup Successful"), tr("The wallet data was successfully saved to %1.").arg(filename),
            CClientUIInterface::MSG_INFORMATION);
    }
}

void WalletView::changePassphrase()
{
    AskPassphraseDialog dlg(AskPassphraseDialog::ChangePass, this);
    dlg.setModel(walletModel);
    dlg.exec();
}

void WalletView::unlockWallet()
{
    if(!walletModel)
        return;
    // Unlock wallet when requested by wallet model
    if (walletModel->getEncryptionStatus() == WalletModel::Locked)
    {
        AskPassphraseDialog dlg(AskPassphraseDialog::Unlock, this);
        dlg.setModel(walletModel);
        dlg.exec();
    }
}

void WalletView::usedSendingAddresses()
{
    if(!walletModel)
        return;

    usedSendingAddressesPage->show();
    usedSendingAddressesPage->raise();
    usedSendingAddressesPage->activateWindow();
}

void WalletView::openStakingRewardSettings()
{
    if(!walletModel)
        return;

    StakingRewardSettingPage dlg(platformStyle, this);
    dlg.setModel(walletModel->getStakingRewardSettingTableModel());
    if(dlg.exec())
    {
        walletModel->wallet().setRewardDistributionPcts(dlg.getReturnValue());
    }
}

void WalletView::usedReceivingAddresses()
{
    if(!walletModel)
        return;

    usedReceivingAddressesPage->show();
    usedReceivingAddressesPage->raise();
    usedReceivingAddressesPage->activateWindow();
}

void WalletView::showProgress(const QString &title, int nProgress)
{
    if (nProgress == 0)
    {
        progressDialog = new QProgressDialog(title, "", 0, 100);
        progressDialog->setWindowModality(Qt::ApplicationModal);
        progressDialog->setMinimumDuration(0);
        progressDialog->setAutoClose(false);
        progressDialog->setValue(0);
        progressDialog->setCancelButtonText(tr("Cancel"));
    }
    else if (nProgress == 100)
    {
        if (progressDialog)
        {
            progressDialog->close();
            progressDialog->deleteLater();
        }
    }
    else if (progressDialog) {
        if (progressDialog->wasCanceled()) {
            getWalletModel()->wallet().abortRescan();
        } else {
            progressDialog->setValue(nProgress);
        }
    }
}

void WalletView::requestedSyncWarningInfo()
{
    Q_EMIT outOfSyncWarningClicked();
}
