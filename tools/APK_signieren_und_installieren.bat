echo zipalign
C:\Users\klaus\AppData\Local\Android\Sdk\build-tools\34.0.0\zipalign.exe -v 4 D:\QtSource\Fahrtenbuch\build\Qt_6_7_3_f_r_Android_arm64_v8a-Release\android-build\build\outputs\apk\release\android-build-release-unsigned.apk D:\QtSource\Fahrtenbuch\build\Qt_6_7_3_f_r_Android_arm64_v8a-Release\android-build\build\outputs\apk\release\aligned.apk
pause
echo signieren
jarsigner -keystore D:\QtSource\Fahrtenbuch\fahrtenbuch-release.jks -signedjar D:\QtSource\Fahrtenbuch\build\Qt_6_7_3_f_r_Android_arm64_v8a-Release\android-build\build\outputs\apk\release\final.apk D:\QtSource\Fahrtenbuch\build\Qt_6_7_3_f_r_Android_arm64_v8a-Release\android-build\build\outputs\apk\release\aligned.apk fahrtenbuch
pause
echo installieren
C:\Users\klaus\AppData\Local\Android\Sdk\platform-tools\adb.exe install -r D:\QtSource\Fahrtenbuch\build\Qt_6_7_3_f_r_Android_arm64_v8a-Release\android-build\build\outputs\apk\release\final.apk
pause
