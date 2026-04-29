// Copyright (c) 2011-2018 The XPChain Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/editstakingrewarddistributiondialog.h>
#include <qt/forms/ui_editstakingrewarddistributiondialog.h>

#include <qt/stakingrewardsettingmodel.h>
#include <qt/guiutil.h>

#include <QDataWidgetMapper>
#include <QMessageBox>
#include <QDialogButtonBox>
#include <QPushButton>

EditStakingRewardDistributionDialog::EditStakingRewardDistributionDialog(Mode _mode, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::EditStakingRewardDistributionDialog),
    mapper(0),
    mode(_mode),
    model(0)
{
    ui->setupUi(this);

    GUIUtil::setupAddressWidget(ui->addressEdit, this);

    switch(mode)
    {
    case New:
        setWindowTitle(tr("New staking reward distribution setting"));
        ui->labelEdit->setEnabled(false);
        break;
    }

    mapper = new QDataWidgetMapper(this);
    mapper->setSubmitPolicy(QDataWidgetMapper::ManualSubmit);

    GUIUtil::ItemDelegate* delegate = new GUIUtil::ItemDelegate(mapper);
    connect(delegate, &GUIUtil::ItemDelegate::keyEscapePressed, this, &EditStakingRewardDistributionDialog::reject);
    mapper->setItemDelegate(delegate);
}

EditStakingRewardDistributionDialog::~EditStakingRewardDistributionDialog()
{
    delete ui;
}

void EditStakingRewardDistributionDialog::setModel(StakingRewardSettingTableModel *_model)
{
    this->model = _model;
    if(!_model)
        return;

    mapper->setModel(_model);
    mapper->addMapping(ui->labelEdit, StakingRewardSettingTableModel::Label);
    mapper->addMapping(ui->addressEdit, StakingRewardSettingTableModel::Address);
    mapper->addMapping(ui->percentageSpinBox, StakingRewardSettingTableModel::DistributionPercentage);
}

void EditStakingRewardDistributionDialog::loadRow(int row)
{
    mapper->setCurrentIndex(row);
}

bool EditStakingRewardDistributionDialog::saveCurrentRow()
{
    if(!model)
        return false;

    switch(mode)
    {
    case New:
        address = model->addRow(
                QString(), // Label is always empty because labelEdit cannot be edited by users
                ui->addressEdit->text(),
                ui->percentageSpinBox->value(),
                model->GetDefaultAddressType());
        break;
    }
    return !address.isEmpty();
}

void EditStakingRewardDistributionDialog::accept()
{
    if(!model)
        return;

    if(!saveCurrentRow())
    {
        StakingRewardSettingTableModel::EditStatus status = model->getEditStatus();
        switch(status)
        {
        case StakingRewardSettingTableModel::OK:
            // Failed with unknown reason. Just reject.
            break;
        case StakingRewardSettingTableModel::NO_CHANGES:
            // No changes were made during edit operation. Just reject.
            break;
        case StakingRewardSettingTableModel::INVALID_ADDRESS:
            QMessageBox::warning(this, windowTitle(),
                tr("The entered address \"%1\" is not a valid XPChain address.").arg(ui->addressEdit->text()),
                QMessageBox::Ok, QMessageBox::Ok);
            break;
        case StakingRewardSettingTableModel::DUPLICATE_ADDRESS:
            QMessageBox::warning(this, windowTitle(),
                getDuplicateAddressWarning(),
                QMessageBox::Ok, QMessageBox::Ok);
            break;
        case StakingRewardSettingTableModel::INVALID_PERCENTAGE:
            QMessageBox::warning(this, windowTitle(),
                tr("The entered number of percentage is not between 1 and 100, or is not a valid number"),
                QMessageBox::Ok, QMessageBox::Ok);
            break;
        }
        return;
    }
    QDialog::accept();
}

QString EditStakingRewardDistributionDialog::getDuplicateAddressWarning() const
{
    QString dup_address = ui->addressEdit->text();
    QString existing_label = model->labelForAddress(dup_address);

    return tr(
        "The entered address \"%1\" is already in the settings with "
        "label \"%2\"."
        ).arg(dup_address).arg(existing_label);
}

QString EditStakingRewardDistributionDialog::getAddress() const
{
    return address;
}

void EditStakingRewardDistributionDialog::setAddress(const QString &_address)
{
    this->address = _address;
    ui->addressEdit->setText(_address);
}
