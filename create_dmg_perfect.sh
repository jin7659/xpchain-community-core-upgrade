#!/bin/bash
set -e

APP_NAME="XPChain-Qt"
APP_BUNDLE="${APP_NAME}.app"
QT_PATH="/opt/homebrew/opt/qt@5"

echo "Building App Bundle..."
rm -rf "${APP_BUNDLE}"
mkdir -p "${APP_BUNDLE}/Contents/MacOS"
mkdir -p "${APP_BUNDLE}/Contents/Resources"
mkdir -p "${APP_BUNDLE}/Contents/Frameworks"
mkdir -p "${APP_BUNDLE}/Contents/PlugIns/platforms"

cp src/qt/xpchain-qt "${APP_BUNDLE}/Contents/MacOS/XPChain-Qt"
chmod +x "${APP_BUNDLE}/Contents/MacOS/XPChain-Qt"

# Copy Cocoa platform plugin
cp "${QT_PATH}/plugins/platforms/libqcocoa.dylib" "${APP_BUNDLE}/Contents/PlugIns/platforms/"

# Copy common dylibs in bulk to save time
echo "Bulk copying libraries..."
cp /opt/homebrew/opt/boost/lib/libboost_*.dylib "${APP_BUNDLE}/Contents/Frameworks/"
cp /opt/homebrew/opt/libevent/lib/libevent*.dylib "${APP_BUNDLE}/Contents/Frameworks/"
cp /opt/homebrew/opt/openssl@3/lib/lib*.dylib "${APP_BUNDLE}/Contents/Frameworks/"
cp /opt/homebrew/opt/berkeley-db@4/lib/libdb_cxx-4.8.dylib "${APP_BUNDLE}/Contents/Frameworks/"
cp /usr/lib/libsqlite3.0.dylib "${APP_BUNDLE}/Contents/Frameworks/" || cp /opt/homebrew/opt/sqlite/lib/libsqlite3.dylib "${APP_BUNDLE}/Contents/Frameworks/"

# Run macdeployqt first for standard Qt bundling
echo "Running macdeployqt..."
${QT_PATH}/bin/macdeployqt "${APP_BUNDLE}" -verbose=1
cp -R ${QT_PATH}/lib/QtNetwork.framework "${APP_BUNDLE}/Contents/Frameworks/"
cp -R ${QT_PATH}/lib/QtPrintSupport.framework "${APP_BUNDLE}/Contents/Frameworks/"
cp -R ${QT_PATH}/lib/QtDBus.framework "${APP_BUNDLE}/Contents/Frameworks/"

# Recursive fix function
fix_deps() {
    local target="$1"
    # echo "Fixing $target..."
    
    otool -L "$target" 2>/dev/null | grep -E "/opt/homebrew|@loader_path" | while read -r line; do
        local dep=$(echo "$line" | awk '{print $1}')
        [ -z "$dep" ] && continue
        
        local libname=$(basename "$dep")
        local actual_source=""

        if [[ "$dep" == /opt/homebrew* ]]; then
            actual_source="$dep"
        elif [[ "$dep" == @loader_path* ]]; then
            # Try to guess source for @loader_path
            if [[ "$libname" == libboost* ]]; then
                actual_source="/opt/homebrew/opt/boost/lib/$libname"
            elif [[ "$libname" == libevent* ]]; then
                actual_source="/opt/homebrew/opt/libevent/lib/$libname"
            elif [[ "$target" == *Qt* ]]; then
                 # Handle Qt internal deps if they use @loader_path
                 actual_source="${QT_PATH}/lib/$libname"
            fi
        fi

        if [ -f "$actual_source" ] && [ ! -f "${APP_BUNDLE}/Contents/Frameworks/$libname" ]; then
             cp "$actual_source" "${APP_BUNDLE}/Contents/Frameworks/"
             chmod +x "${APP_BUNDLE}/Contents/Frameworks/$libname"
        fi

        if [ -f "${APP_BUNDLE}/Contents/Frameworks/$libname" ]; then
            install_name_tool -change "$dep" "@executable_path/../Frameworks/$libname" "$target" 2>/dev/null || true
        elif [ -d "${APP_BUNDLE}/Contents/Frameworks/${libname}.framework" ]; then
             # Handle frameworks
             local fw_name="${libname}.framework"
             install_name_tool -change "$dep" "@executable_path/../Frameworks/${fw_name}/Versions/5/${libname}" "$target" 2>/dev/null || true
        fi
    done
}

echo "Fixing all files..."
# Process binary, all dylibs and all nested dylibs in frameworks
find "${APP_BUNDLE}" -type f \( -name "XPChain-Qt" -o -name "*.dylib" -o -perm +111 \) | while read -r file; do
    fix_deps "$file"
done

# Second pass for frameworks internal structure
find "${APP_BUNDLE}/Contents/Frameworks" -name "Qt*" -type f | while read -r file; do
    fix_deps "$file"
done

echo "Signing..."
codesign --force --deep --sign - "${APP_BUNDLE}"

echo "DMG..."
hdiutil create -volname "${APP_NAME}" -srcfolder "${APP_BUNDLE}" -ov -format UDZO "${APP_NAME}.dmg"

echo "Done."
