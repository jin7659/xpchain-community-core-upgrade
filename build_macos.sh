#!/bin/bash
set -e

# Define paths
QT_PATH="/opt/homebrew/opt/qt@5"
BDB_PATH="/opt/homebrew/opt/berkeley-db@4"
BOOST_PATH="/opt/homebrew"

export PATH="${QT_PATH}/bin:${PATH}"
export PKG_CONFIG_PATH="${QT_PATH}/lib/pkgconfig:${PKG_CONFIG_PATH}"
export LDFLAGS="-L${QT_PATH}/lib -L${BDB_PATH}/lib"
export CPPFLAGS="-I${QT_PATH}/include -I${BDB_PATH}/include"

echo "Running configure with explicit paths..."
./configure \
    --with-incompatible-bdb \
    --with-gui=qt5 \
    --with-boost=${BOOST_PATH} \
    --disable-shared \
    --with-pic \
    --with-bignum=no \
    --enable-module-recovery \
    --enable-module-schnorrsig \
    --enable-module-extrakeys \
    --disable-jni \
    MOC="${QT_PATH}/bin/moc" \
    UIC="${QT_PATH}/bin/uic" \
    RCC="${QT_PATH}/bin/rcc" \
    LRELEASE="${QT_PATH}/bin/lrelease"

echo "Checking if ENABLE_QT is true in config.status..."
grep "ENABLE_QT_TRUE" config.status

echo "Force building GUI object files..."
make -C src qt/xpchain_qt-xpchain.o qt/libxpchainqt_a-xpchaingui.o -j$(sysctl -n hw.ncpu)

echo "Building the final GUI binary..."
make -C src qt/xpchain-qt -j$(sysctl -n hw.ncpu)

echo "Verifying the new binary..."
ls -l src/qt/xpchain-qt
strings src/qt/xpchain-qt | grep "v0.27.0.0-FINAL"

echo "Creating the new DMG..."
bash package_dmg.sh
