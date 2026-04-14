::app auf smartphone beendden
rem C:\Users\klaus\AppData\Local\Android\Sdk\platform-tools\adb.exe shell am force-stop com.fahrtenbuch.app
rem C:\Users\klaus\AppData\Local\Android\Sdk\platform-tools\adb.exe logcat | findstr -i "fahrtenbuch\|qt\|FATAL\|AndroidRuntime"
::ohne filter
remC:\Users\klaus\AppData\Local\Android\Sdk\platform-tools\adb.exe logcat -c
::ohne filter
rem C:\Users\klaus\AppData\Local\Android\Sdk\platform-tools\adb.exe logcat *:E

::ADB komplett neu starten
C:\Users\klaus\AppData\Local\Android\Sdk\platform-tools\adb.exe kill-server
C:\Users\klaus\AppData\Local\Android\Sdk\platform-tools\adb.exe start-server
C:\Users\klaus\AppData\Local\Android\Sdk\platform-tools\adb.exe shell am force-stop com.fahrtenbuch.app

::App deinstallieren
C:\Users\klaus\AppData\Local\Android\Sdk\platform-tools\adb.exe uninstall com.fahrtenbuch.app
