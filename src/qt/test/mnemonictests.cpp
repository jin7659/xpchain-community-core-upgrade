#include <qt/test/mnemonictests.h>

#include <qt/mnemonic.h>
#include <support/allocators/secure.h>
#include <utilstrencodings.h>

#include <string>
#include <vector>

#include <QTest>

void MnemonicTests::bip39Valid12Word()
{
    const SecureString mnemonic = "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about";
    std::string error;
    QVERIFY(Mnemonic::Validate(mnemonic, error));
    QVERIFY(error.empty());
}

void MnemonicTests::bip39InvalidChecksum()
{
    const SecureString mnemonic = "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon";
    std::string error;
    QVERIFY(!Mnemonic::Validate(mnemonic, error));
    QVERIFY(!error.empty());
}

void MnemonicTests::bip39DeriveSeedDeterministic()
{
    const SecureString mnemonic = "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about";
    const SecureString passphrase;
    std::vector<unsigned char> seed = Mnemonic::DeriveSeed(mnemonic, passphrase);
    QCOMPARE(seed.size(), size_t(64));
    QCOMPARE(HexStr(seed), std::string("5eb00bbddcf069084889a8ab9155568165f5c453ccb85e70811aaed6f6da5fc19a5ac40b389cd370d086206dec8aa6c43daea6690f20ad3d8d48b2d2ce9e38e4"));
}
