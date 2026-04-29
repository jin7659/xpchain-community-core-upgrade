#!/bin/bash
set -e

APP_NAME="XPChain-Qt"
APP_BUNDLE="${APP_NAME}.app"
QT_BIN="/opt/homebrew/opt/qt@5/bin"
MACDEPLOYQT="${QT_BIN}/macdeployqt"

echo "Building App Bundle..."
# Ensure app bundle dirs exist
mkdir -p "${APP_BUNDLE}/Contents/MacOS"
mkdir -p "${APP_BUNDLE}/Contents/Resources"

# Copy binary
cp src/qt/xpchain-qt "${APP_BUNDLE}/Contents/MacOS/XPChain-Qt"
chmod +x "${APP_BUNDLE}/Contents/MacOS/XPChain-Qt"

# Copy Icon if exists
if [ -f "src/qt/res/icons/xpchain.icns" ]; then
    cp src/qt/res/icons/xpchain.icns "${APP_BUNDLE}/Contents/Resources/xpchain.icns"
fi

# Copy Info.plist
if [ -f "share/qt/Info.plist" ]; then
    cp share/qt/Info.plist "${APP_BUNDLE}/Contents/Info.plist"
else
    # Create a basic Info.plist if missing
    cat <<EOF > "${APP_BUNDLE}/Contents/Info.plist"
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleExecutable</key>
	<string>XPChain-Qt</string>
	<key>CFBundleIconFile</key>
	<string>xpchain.icns</string>
	<key>CFBundleIdentifier</key>
	<string>org.xpchain.XPChain-Qt</string>
	<key>CFBundleName</key>
	<string>XPChain Core</string>
	<key>CFBundlePackageType</key>
	<string>APPL</string>
	<key>CFBundleShortVersionString</key>
	<string>0.17.0</string>
	<key>LSMinimumSystemVersion</key>
	<string>10.12</string>
</dict>
</plist>
EOF
fi

echo "Running macdeployqt..."
"${MACDEPLOYQT}" "${APP_BUNDLE}" -verbose=1

echo "Fixing Homebrew dependencies..."
FRAMEWORKS_DIR="${APP_BUNDLE}/Contents/Frameworks"
mkdir -p "${FRAMEWORKS_DIR}"

# Function to fix a dylib and its dependencies
fix_lib() {
    local target="$1"
    echo "Fixing $target..."
    
    # Get direct dependencies from homebrew
    otool -L "$target" | grep "/opt/homebrew" | while read -r line; do
        local dep=$(echo "$line" | awk '{print $1}')
        local libname=$(basename "$dep")
        
        if [ ! -f "${FRAMEWORKS_DIR}/$libname" ]; then
            cp "$dep" "${FRAMEWORKS_DIR}/"
            chmod +x "${FRAMEWORKS_DIR}/$libname"
            fix_lib "${FRAMEWORKS_DIR}/$libname"
        fi
        
        install_name_tool -change "$dep" "@executable_path/../Frameworks/$libname" "$target"
    done
}

# Initial fix for the main binary
chmod +x "${APP_BUNDLE}/Contents/MacOS/XPChain-Qt"
fix_lib "${APP_BUNDLE}/Contents/MacOS/XPChain-Qt"

echo "Creating DMG..."
hdiutil create -volname "${APP_NAME}" -srcfolder "${APP_BUNDLE}" -ov -format UDZO "${APP_NAME}.dmg"

echo "Done. DMG created."
