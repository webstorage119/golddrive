:: SSHFS
:: Mount linux filesystem in windows drive
:: Usage: sshfs.bat [host] [drive]
:: 10/30/2018, sganis
::
:: golddrive keys must be setup

@echo off
setlocal
 
if [%4]==[] goto :usage
if "%1"=="-h" goto :usage

set USER=%1
set HOST=%2
set PORT=%3
set DRIVE=%4
 
:: this script directory
set DIR=%~dp0
set DIR=%DIR:~0,-1%


set USERHOST=%USER%@%HOST%

rem set SSH=%DIR%\..\..\sshfs-win\.build\x64\root\bin
rem set BUILD=%DIR%\..\..\sshfs-win\.build\x64\src\sshfs\build
rem set PATH=%BUILD%;%SSH%;%PATH%

set "SSH=C:\Program Files (x86)\SSHFS-Win\bin"
set PATH=%SSH%;%PATH%

where ssh
where sshfs

sshfs.exe %USERHOST%:../.. %DRIVE%  ^
   -o port=%PORT% ^
   -o IdentityFile=%USERPROFILE:\=/%/.ssh/id_rsa ^
   -o VolumePrefix=/sshfs.k/%USERHOST%/../.. ^
   -o uid=-1,gid=-1,create_umask=000,mask=000 ^
   -o PasswordAuthentication=no ^
   -o StrictHostKeyChecking=no ^
   -o UserKnownHostsFile=/dev/null ^
   -o compression=no ^
   -o rellinks ^
   -o dothidden ^
   -o reconnect ^
   -f
   
rem -o dir_cache=no ^
   
goto :eof
 
:usage
echo usage: %0 [user] [host] [port] [drive]
exit /B 1
