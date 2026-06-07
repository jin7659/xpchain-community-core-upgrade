#ifndef XPCHAIN_QT_TEST_MNEMONICTESTS_H
#define XPCHAIN_QT_TEST_MNEMONICTESTS_H

#include <QObject>

class MnemonicTests : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void bip39Valid12Word();
    void bip39InvalidChecksum();
    void bip39DeriveSeedDeterministic();
};

#endif // XPCHAIN_QT_TEST_MNEMONICTESTS_H
