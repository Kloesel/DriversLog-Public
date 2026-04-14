cd D:\QtSource\Fahrtenbuch\build\Qt_6_7_3_f_r_Android_arm64_v8a-Release\android-build\build\outputs\apk\release
jarsigner -keystore D:\QtSource\Fahrtenbuch\fahrtenbuch-release.jks -signedjar signed.apk android-build-release-unsigned.apk fahrtenbuch
