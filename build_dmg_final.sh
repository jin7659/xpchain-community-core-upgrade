#!/bin/bash
set -e

APP_NAME="XPChain-Qt"
APP_BUNDLE="${APP_NAME}.app"
APP_EXE="${APP_BUNDLE}/Contents/MacOS/${APP_NAME}"
QT_PATH="/opt/homebrew/opt/qt@5"
DMG_NAME="${APP_NAME}.dmg"
VOL_NAME="${APP_NAME}"
BACKGROUND="background.tiff"

echo "Step 1: Cleaning up..."
rm -rf "${APP_BUNDLE}"
rm -rf "dmg_temp"
rm -f "${DMG_NAME}"
rm -f "tmp.dmg"

echo "Step 2: Preparing App Bundle..."
mkdir -p "${APP_BUNDLE}/Contents/MacOS"
mkdir -p "${APP_BUNDLE}/Contents/Resources"
mkdir -p "${APP_BUNDLE}/Contents/Frameworks"

cp src/qt/xpchain-qt "${APP_EXE}"
chmod +x "${APP_EXE}"

# Create Info.plist if missing
cat <<EOF > "${APP_BUNDLE}/Contents/Info.plist"
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleExecutable</key>
	<string>${APP_NAME}</string>
	<key>CFBundleIconFile</key>
	<string>bitcoin.icns</string>
	<key>CFBundleIdentifier</key>
	<string>co.kr.xpchain.XPChain-Qt</string>
	<key>CFBundleName</key>
	<string>${APP_NAME}</string>
	<key>CFBundlePackageType</key>
	<string>APPL</string>
	<key>LSMinimumSystemVersion</key>
	<string>10.15</string>
</dict>
</plist>
EOF

if [ -f "src/qt/res/icons/xpchain.icns" ]; then
    cp "src/qt/res/icons/xpchain.icns" "${APP_BUNDLE}/Contents/Resources/bitcoin.icns"
fi

echo "Step 3: Running macdeployqt..."
# We use macdeployqt to handle Qt frameworks and plugins
"${QT_PATH}/bin/macdeployqt" "${APP_BUNDLE}" -verbose=1

echo "Step 4: Fixing non-Qt dependencies recursively..."

# Robust dependency fixer
fix_deps() {
    local target="$1"
    # echo "Processing $target..."
    
    # We look for homebrew paths and @loader_path references
    otool -L "$target" 2>/dev/null | grep -E "/opt/homebrew|@loader_path" | while read -r line; do
        local dep=$(echo "$line" | awk '{print $1}')
        [ -z "$dep" ] && continue
        
        local libname=$(basename "$dep")
        # Skip if it's the library itself (ID)
        [ "$(basename "$target")" == "$libname" ] && continue

        local dest_lib="${APP_BUNDLE}/Contents/Frameworks/$libname"
        
        if [ ! -f "$dest_lib" ]; then
            local actual_source=""
            if [[ "$dep" == /opt/homebrew* ]]; then
                actual_source="$dep"
            else
                # Try to find in /opt/homebrew/lib or other common spots
                for search_path in "/opt/homebrew/lib" "/opt/homebrew/opt/boost/lib" "/opt/homebrew/opt/libevent/lib" "/opt/homebrew/opt/openssl@3/lib" "/opt/homebrew/opt/berkeley-db@4/lib" "/opt/homebrew/opt/abseil/lib" "/opt/homebrew/opt/protobuf/lib"; do
                    if [ -f "$search_path/$libname" ]; then
                        actual_source="$search_path/$libname"
                        break
                    fi
                done
            fi

            if [ -n "$actual_source" ] && [ -f "$actual_source" ]; then
                echo "  Copying $libname from $actual_source..."
                cp "$actual_source" "$dest_lib"
                chmod +x "$dest_lib"
                # Recurse
                fix_deps "$dest_lib"
            fi
        fi

        if [ -f "$dest_lib" ]; then
            # Change reference to use @executable_path so it works everywhere
            install_name_tool -change "$dep" "@executable_path/../Frameworks/$libname" "$target" 2>/dev/null || true
        fi
    done
}

fix_deps "${APP_EXE}"
# Fix all dylibs in Frameworks to ensure they use @executable_path relative to MacOS
find "${APP_BUNDLE}/Contents/Frameworks" -name "*.dylib" | while read -r lib; do
    fix_deps "$lib"
    install_name_tool -id "@executable_path/../Frameworks/$(basename "$lib")" "$lib" 2>/dev/null || true
done

echo "Step 5: Signing..."
codesign --force --deep --sign - "${APP_BUNDLE}"

echo "Step 6: Creating Premium DMG..."
mkdir -p "dmg_temp"
cp -R "${APP_BUNDLE}" "dmg_temp/"
ln -s /Applications "dmg_temp/Applications"
mkdir -p "dmg_temp/.background"
cp "${BACKGROUND}" "dmg_temp/.background/background.tiff"

hdiutil create -srcfolder "dmg_temp" -volname "${VOL_NAME}" -fs HFS+ -fsargs "-c c=64,a=16,e=16" -format UDRW -size 400m "tmp.dmg"
rm -rf "dmg_temp"

echo "  Mounting DMG..."
# Unmount if already mounted
hdiutil detach "/Volumes/${VOL_NAME}" 2>/dev/null || true
sleep 1
device=$(hdiutil attach -readwrite -noverify "tmp.dmg" | egrep '^/dev/' | sed 1q | awk '{print $1}')
sleep 5

echo "  Applying layout via AppleScript..."
osascript <<EOF
tell application "Finder"
  -- Wait for the disk to be available
  set mounted_disk to "${VOL_NAME}"
  set i to 0
  repeat while not (exists disk mounted_disk) and i < 10
    delay 1
    set i to i + 1
  end repeat

  tell disk mounted_disk
    open
    set theViewOptions to the icon view options of container window
    set background picture of theViewOptions to POSIX file "/Volumes/${VOL_NAME}/.background/background.tiff"
    set arrangement of theViewOptions to not arranged
    set icon size of theViewOptions to 96
    set position of item "${APP_NAME}.app" to {130, 160}
    set position of item "Applications" to {370, 160}
    set the bounds of container window to {400, 100, 900, 420}
    delay 2
    close
  end tell
end tell
EOF

echo "  Cleaning up and Finalizing DMG..."
sleep 2
hdiutil detach "${device}" -force
hdiutil convert "tmp.dmg" -format UDZO -imagekey zlib-level=9 -o "${DMG_NAME}"
rm "tmp.dmg"

echo "Done! Final DMG: ${DMG_NAME}"
