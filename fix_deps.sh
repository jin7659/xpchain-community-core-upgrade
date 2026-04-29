#!/bin/bash
cd /Users/jeongjinseob/Desktop/Antigravity/xpc_upgrade/dist/XPChain-Qt.app/Contents/Frameworks || exit 1
while true; do
  MISSING=$(otool -L *.dylib ../MacOS/XPChain-Qt 2>/dev/null | grep "/opt/homebrew" | awk '{print $1}' | sort | uniq | grep -v ":$")
  NEW_COPIED=0
  for lib in $MISSING; do
    basename=$(basename $lib)
    if [ ! -f "$basename" ]; then
      echo "Copying $lib"
      cp "$lib" .
      chmod +w "$basename"
      NEW_COPIED=1
    fi
  done
  if [ "$NEW_COPIED" -eq 0 ]; then
    break
  fi
done

for dylib in *.dylib ../MacOS/XPChain-Qt; do
  if [ -f "$dylib" ]; then
    if [[ "$dylib" == *.dylib ]]; then
      install_name_tool -id "@executable_path/../Frameworks/$(basename $dylib)" "$dylib" 2>/dev/null || true
    fi
    otool -L "$dylib" 2>/dev/null | grep -E "/opt/homebrew|@loader_path/libboost|@rpath" | awk '{print $1}' | grep -v ":$" | while read ref; do
      if [[ "$ref" == /opt/homebrew* ]] || [[ "$ref" == @loader_path/libboost* ]]; then
          install_name_tool -change "$ref" "@executable_path/../Frameworks/$(basename $ref)" "$dylib" 2>/dev/null || true
      elif [[ "$ref" == @rpath* ]]; then
          # For rpath libs, macdeployqt usually doesn't rewrite them to executable_path if they are plugins, but for frameworks we can.
          # Only change it if we actually copied the file here!
          basename=$(basename $ref)
          if [ -f "$basename" ]; then
              install_name_tool -change "$ref" "@executable_path/../Frameworks/$basename" "$dylib" 2>/dev/null || true
          fi
      fi
    done
  fi
done
echo "Dependencies fixed."
