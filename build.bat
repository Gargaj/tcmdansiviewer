@echo off

for /f "tokens=2 delims==" %%I in ('wmic os get localdatetime /format:list') do set DATETIME=%%I

set FN=ansi_wlx_%DATETIME:~0,8%.zip

pushd Release\
zip -9 -D ../%FN% tcmdansiviewer.wlx
popd

pushd x64\Release\
zip -9 -D ../../%FN% tcmdansiviewer.wlx64
popd

zip -9 %FN% readme.txt
zip -9 %FN% pluginst.inf
