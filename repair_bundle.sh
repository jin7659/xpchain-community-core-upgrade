#!/bin/bash
set -e

APP_BUNDLE="XPChain-Qt.app"
FRAMEWORKS_DIR="${APP_BUNDLE}/Contents/Frameworks"

repair_lib() {
    local target="$1"
    # echo "Repairing $target..."
    
    otool -L "$target" 2>/dev/null | grep -E "/opt/homebrew|@loader_path" | while read -r line; do
        local dep=$(echo "$line" | awk '{print $1}')
        [ -z "$dep" ] && continue
        
        local libname=$(basename "$dep")
        local actual_source=""

        if [[ "$dep" == /opt/homebrew* ]]; then
            actual_source="$dep"
        elif [[ "$dep" == @loader_path* ]]; then
             # Common homebrew paths for boost/libevent/openssl
             for base in boost libevent openssl@3 berkeley-db@4; do
                 if [ -f "/opt/homebrew/opt/$base/lib/$libname" ]; then
                     actual_source="/opt/homebrew/opt/$base/lib/$libname"
                     break
                 fi
             done
        fi

        if [ -f "$actual_source" ] && [ ! -f "${FRAMEWORKS_DIR}/$libname" ]; then
             echo "Copying missing dependency: $libname"
             cp "$actual_source" "${FRAMEWORKS_DIR}/"
             chmod +x "${FRAMEWORKS_DIR}/$libname"
             repair_lib "${FRAMEWORKS_DIR}/$libname"
        fi

        if [ -f "${FRAMEWORKS_DIR}/$libname" ]; then
            install_name_tool -change "$dep" "@executable_path/../Frameworks/$libname" "$target" 2>/dev/null || true
        fi
    done
}

echo "Repairing bundle dependencies..."
find "${APP_BUNDLE}" -type f \( -name "XPChain-Qt" -o -name "*.dylib" \) | while read -r file; do
    repair_lib "$file"
done

# Fix IDs
for lib in "${FRAMEWORKS_DIR}"/*.dylib; do
    if [ -f "$lib" ]; then
        install_name_tool -id "@executable_path/../Frameworks/$(basename "$lib")" "$lib" 2>/dev/null || true
    fi
done

echo "Signing..."
codesign --force --deep --sign - "${APP_BUNDLE}"

echo "DMG..."
hdiutil create -volname "XPChain-Qt" -srcfolder "${APP_BUNDLE}" -ov -format UDZO "XPChain-Qt.dmg"

echo "Done."
