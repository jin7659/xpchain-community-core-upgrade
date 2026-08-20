// Copyright (c) 2026 The XPChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XPCHAIN_QT_IMPORTADDRESSDIALOG_H
#define XPCHAIN_QT_IMPORTADDRESSDIALOG_H

#include <QDialog>

class QCheckBox;
class QLineEdit;

/** Import a watch-only address into the current wallet (RPC importaddress). */
class ImportAddressDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ImportAddressDialog(QWidget* parent = nullptr);

    QString address() const;
    QString label() const;
    bool rescan() const;

private:
    QLineEdit* m_addressEdit;
    QLineEdit* m_labelEdit;
    QCheckBox* m_rescanCheck;
};

#endif // XPCHAIN_QT_IMPORTADDRESSDIALOG_H
