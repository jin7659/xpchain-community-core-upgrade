#!/bin/bash
set -e

APP_NAME="XPChain-Core"
APP_BUNDLE="${APP_NAME}.app"
DMG_NAME="XPChain-Core.dmg"
BACKGROUND_IMG="contrib/macdeploy/background_pure_white.png"
VOL_NAME="XPChain Core Installation"
QT_PATH="/opt/homebrew/opt/qt@5"

# 1. Clean up
hdiutil detach "/Volumes/${VOL_NAME}" -force 2>/dev/null || true
rm -f "${DMG_NAME}"
rm -rf "${APP_BUNDLE}"
rm -rf "tmp_dmg"
mkdir -p "tmp_dmg"

# 2. Build App Bundle
echo "Building App Bundle..."
mkdir -p "${APP_BUNDLE}/Contents/MacOS"
mkdir -p "${APP_BUNDLE}/Contents/Resources"
mkdir -p "${APP_BUNDLE}/Contents/Frameworks"
mkdir -p "${APP_BUNDLE}/Contents/PlugIns/platforms"

cp src/qt/xpchain-qt "${APP_BUNDLE}/Contents/MacOS/XPChain-Qt"
chmod +x "${APP_BUNDLE}/Contents/MacOS/XPChain-Qt"
cp src/qt/res/icons/xpchain.icns "${APP_BUNDLE}/Contents/Resources/"
cp share/qt/Info.plist "${APP_BUNDLE}/Contents/Info.plist"
sed -i '' 's/bitcoin.icns/xpchain.icns/g' "${APP_BUNDLE}/Contents/Info.plist"

# Copy Cocoa platform plugin
cp "${QT_PATH}/plugins/platforms/libqcocoa.dylib" "${APP_BUNDLE}/Contents/PlugIns/platforms/"

# Copy common dylibs
echo "Copying libraries..."
cp /opt/homebrew/opt/boost/lib/libboost_*.dylib "${APP_BUNDLE}/Contents/Frameworks/"
cp /opt/homebrew/opt/libevent/lib/libevent*.dylib "${APP_BUNDLE}/Contents/Frameworks/"
cp /opt/homebrew/opt/openssl@3/lib/lib*.dylib "${APP_BUNDLE}/Contents/Frameworks/"
cp /opt/homebrew/opt/berkeley-db@4/lib/libdb_cxx-4.8.dylib "${APP_BUNDLE}/Contents/Frameworks/"
cp /opt/homebrew/opt/sqlite/lib/libsqlite3.dylib "${APP_BUNDLE}/Contents/Frameworks/" 2>/dev/null || true

# Run macdeployqt
echo "Running macdeployqt..."
${QT_PATH}/bin/macdeployqt "${APP_BUNDLE}" -verbose=1
cp -R ${QT_PATH}/lib/QtNetwork.framework "${APP_BUNDLE}/Contents/Frameworks/" 2>/dev/null || true
cp -R ${QT_PATH}/lib/QtPrintSupport.framework "${APP_BUNDLE}/Contents/Frameworks/" 2>/dev/null || true
cp -R ${QT_PATH}/lib/QtDBus.framework "${APP_BUNDLE}/Contents/Frameworks/" 2>/dev/null || true

# 3. Create temporary disk image
echo "Creating temporary disk image..."
hdiutil create -size 400m -fs HFS+ -volname "${VOL_NAME}" -ov "tmp.dmg"

# 4. Mount it
echo "Mounting..."
device=$(hdiutil attach -readwrite -noverify "tmp.dmg" | grep '^/dev/' | head -n 1 | awk '{print $1}')
sleep 2
mount_point="/Volumes/${VOL_NAME}"

# 5. Copy content to DMG
echo "Copying content to DMG volume..."
cp -R "${APP_BUNDLE}" "${mount_point}/"
ln -s /Applications "${mount_point}/Applications"

# 6. Set background
echo "Setting background..."
mkdir "${mount_point}/.background"
cp "${BACKGROUND_IMG}" "${mount_point}/.background/background.png"

# 7. Apply AppleScript for styling
echo "Applying styling with AppleScript..."
osascript <<EOF
tell application "Finder"
    tell disk "${VOL_NAME}"
        open
        set current view of container window to icon view
        set toolbar visible of container window to false
        set statusbar visible of container window to false
        set the bounds of container window to {400, 100, 1400, 900}
        set viewOptions to the icon view options of container window
        set icon size of viewOptions to 128
        set arrangement of viewOptions to not arranged
        set background picture of viewOptions to file ".background:background.png"
        
        delay 1
        
        # Position icons
        set position of item "${APP_NAME}.app" to {250, 480}
        set position of item "Applications" to {785, 480}
        
        update without registering applications
        delay 2
        close
    end tell
end tell
EOF

# 8. Finalize DMG
echo "Finalizing DMG..."
hdiutil detach "${device}"
hdiutil convert "tmp.dmg" -format UDZO -o "${DMG_NAME}" -ov
rm "tmp.dmg"

echo "Success! ${DMG_NAME} created."
