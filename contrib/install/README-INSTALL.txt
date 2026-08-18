XPChain Core — install from release archive
============================================

Linux (x86_64 tar.gz)
---------------------
1. Extract:  tar -xzf xpchain-*-linux-x86_64.tar.gz && cd xpchain-*-linux-x86_64
2. Install:    ./install.sh
3. Add PATH:   export PATH="$HOME/.local/xpchain/bin:$PATH"   (add to ~/.bashrc)
4. Run GUI:    xpchain-qt
5. Uninstall:  ./uninstall.sh

macOS (.dmg)
------------
1. Open the .dmg and drag **XPChain-Core.app** to **Applications**.
2. First launch: right-click the app → **Open** (ad-hoc signed builds).
3. CLI tools are inside the .app bundle or use the companion .tar.gz binaries.

Windows (zip or setup.exe)
--------------------------
**Setup.exe (recommended):** run the NSIS installer and follow the wizard.

**Zip:** extract, then either:
  - Double-click **install.bat** (Administrator), or
  - PowerShell:  .\install.ps1

Data directory defaults to %APPDATA%\XPChain on Windows, ~/.xpchain on Linux/macOS.

This preview build is not a formal signed v0.27.0 release (IS_RELEASE=false).
