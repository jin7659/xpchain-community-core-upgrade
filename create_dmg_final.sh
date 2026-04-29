#!/bin/bash
set -e

APP_NAME="XPChain-Qt"
APP_BUNDLE="${APP_NAME}.app"

echo "Building App Bundle..."
mkdir -p "${APP_BUNDLE}/Contents/MacOS"
mkdir -p "${APP_BUNDLE}/Contents/Resources"
mkdir -p "${APP_BUNDLE}/Contents/Frameworks"

cp src/qt/xpchain-qt "${APP_BUNDLE}/Contents/MacOS/XPChain-Qt"
chmod +x "${APP_BUNDLE}/Contents/MacOS/XPChain-Qt"

# Pre-emptively copy ALL boost and libevent libs because they are tricky
echo "Pre-copying common dependencies..."
cp /opt/homebrew/opt/boost/lib/libboost_*.dylib "${APP_BUNDLE}/Contents/Frameworks/"
cp /opt/homebrew/opt/libevent/lib/libevent*.dylib "${APP_BUNDLE}/Contents/Frameworks/"
cp /opt/homebrew/opt/openssl@3/lib/lib*.dylib "${APP_BUNDLE}/Contents/Frameworks/"
cp /opt/homebrew/opt/berkeley-db@4/lib/libdb_cxx-4.8.dylib "${APP_BUNDLE}/Contents/Frameworks/"

chmod +x "${APP_BUNDLE}/Contents/Frameworks"/*.dylib

# Recursive fix function
fix_deps() {
    local target="$1"
    echo "Fixing $target..."
    
    otool -L "$target" | grep -E "/opt/homebrew|@loader_path" | while read -r line; do
        local dep=$(echo "$line" | awk '{print $1}')
        [ -z "$dep" ] && continue
        [ "$dep" == "$target:" ] && continue
        
        local libname=$(basename "$dep")
        
        # If it's the library itself, ignore
        if [[ "$dep" == *"$libname"* && "$target" == *"$libname"* ]]; then
            continue
        fi

        # If it's in our Frameworks already, just update the path
        if [ -f "${APP_BUNDLE}/Contents/Frameworks/$libname" ]; then
            install_name_tool -change "$dep" "@executable_path/../Frameworks/$libname" "$target"
        fi
    done
}

echo "Fixing main binary..."
fix_deps "${APP_BUNDLE}/Contents/MacOS/XPChain-Qt"

echo "Fixing all bundled libraries..."
for i in {1..3}; do # Run multiple times to catch nested dependencies
    for lib in "${APP_BUNDLE}/Contents/Frameworks"/*.dylib; do
        fix_deps "$lib"
        install_name_tool -id "@executable_path/../Frameworks/$(basename "$lib")" "$lib"
    done
done

echo "Signing..."
codesign --force --deep --sign - "${APP_BUNDLE}"

echo "DMG..."
hdiutil create -volname "${APP_NAME}" -srcfolder "${APP_BUNDLE}" -ov -format UDZO "${APP_NAME}.dmg"

echo "Done."
