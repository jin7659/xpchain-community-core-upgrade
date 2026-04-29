// Copyright (c) 2011-2014 The XPChain Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XPCHAIN_QT_BITCOINADDRESSVALIDATOR_H
#define XPCHAIN_QT_BITCOINADDRESSVALIDATOR_H

#include <QValidator>

/** Base58 entry widget validator, checks for valid characters and
 * removes some whitespace.
 */
class XPChainAddressEntryValidator : public QValidator
{
    Q_OBJECT

public:
    explicit XPChainAddressEntryValidator(QObject *parent);

    State validate(QString &input, int &pos) const;
};

/** XPChain address widget validator, checks for a valid xpchain address.
 */
class XPChainAddressCheckValidator : public QValidator
{
    Q_OBJECT

public:
    explicit XPChainAddressCheckValidator(QObject *parent);

    State validate(QString &input, int &pos) const;
};

#endif // XPCHAIN_QT_BITCOINADDRESSVALIDATOR_H
