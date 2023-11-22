@echo off

set arg0=%~1
set arg1=%~2

set installer_exe=%arg0%
set install_dst=%arg1:~0,-1%

%installer_exe% /S /D=%install_dst%

exit