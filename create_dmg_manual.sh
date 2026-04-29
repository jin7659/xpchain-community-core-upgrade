#!/bin/bash
set -x
set -e

APP_NAME="XPChain-Qt"
APP_BUNDLE="${APP_NAME}.app"

echo "Building App Bundle..."
mkdir -p "${APP_BUNDLE}/Contents/MacOS"
mkdir -p "${APP_BUNDLE}/Contents/Resources"
mkdir -p "${APP_BUNDLE}/Contents/Frameworks"

cp src/qt/xpchain-qt "${APP_BUNDLE}/Contents/MacOS/XPChain-Qt"
chmod +x "${APP_BUNDLE}/Contents/MacOS/XPChain-Qt"

# Function to recursively fix dependencies
fix_lib() {
    local target="$1"
    echo "Processing $target..."
    
    # Get dependencies (both absolute homebrew paths and @loader_path)
    otool -L "$target" | grep -E "/opt/homebrew|@loader_path" | grep -v "$(basename "$target")" | while read -r line; do
        local dep=$(echo "$line" | awk '{print $1}')
        local libname=$(basename "$dep")
        local actual_source=""

        if [[ "$dep" == /opt/homebrew* ]]; then
            actual_source="$dep"
        elif [[ "$dep" == @loader_path* ]]; then
            # If target is in Frameworks, we need to know where it came from
            # For simplicity, we assume boost libs are in the standard homebrew path
            actual_source="/opt/homebrew/opt/boost/lib/$libname"
        fi

        if [ -f "$actual_source" ]; then
            if [ ! -f "${APP_BUNDLE}/Contents/Frameworks/$libname" ]; then
                echo "Copying $libname from $actual_source..."
                cp "$actual_source" "${APP_BUNDLE}/Contents/Frameworks/"
                chmod +x "${APP_BUNDLE}/Contents/Frameworks/$libname"
                fix_lib "${APP_BUNDLE}/Contents/Frameworks/$libname"
            fi
            install_name_tool -change "$dep" "@executable_path/../Frameworks/$libname" "$target"
        fi
    done
}

echo "Fixing dependencies recursively..."
fix_lib "${APP_BUNDLE}/Contents/MacOS/XPChain-Qt"

# Also fix the IDs of the copied libraries
for lib in "${APP_BUNDLE}/Contents/Frameworks"/*.dylib; do
    if [ -f "$lib" ]; then
        install_name_tool -id "@executable_path/../Frameworks/$(basename "$lib")" "$lib"
    fi
done

echo "Signing the bundle..."
codesign --force --deep --sign - "${APP_BUNDLE}"

echo "Creating DMG..."
hdiutil create -volname "${APP_NAME}" -srcfolder "${APP_BUNDLE}" -ov -format UDZO "${APP_NAME}.dmg"

echo "Done."
