rem
rem

echo off
PATH=%PATH%;openssl\lib\;bulid\;
RD /S /Q examples
XCOPY ..\examples .\examples /Y /E /I /Q
%*
