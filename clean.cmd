@echo off

setlocal
call :setESC

CALL :printHeader, "Removing folders"
call :removeFolder, "external"
call :removeFolder, "AirLib\deps"
call :removeFolder, "AirLib\temp"
call :removeFolder, "AirLib\lib"
call :removeFolder, "Unreal\Plugins\AirSim\Content\VehicleAdv\SUV"
ECHO(   

CALL :printHeader, "Cleaning visual studio solutions: Configuration = Debug"
msbuild /p:Platform=x64 /p:Configuration=Debug AirSim.sln /t:Clean
if ERRORLEVEL 1 goto :buildfailed
ECHO(   

CALL :printHeader, "Cleaning visual studio solutions: Configuration = Release"
msbuild /p:Platform=x64 /p:Configuration=Release AirSim.sln /t:Clean
if ERRORLEVEL 1 goto :buildfailed
ECHO(   

CALL :printHeader, "Cleaning visual studio solutions: Configuration = RelWithDebInfo"
msbuild /p:Platform=x64 /p:Configuration=RelWithDebInfo AirSim.sln /t:Clean
if ERRORLEVEL 1 goto :buildfailed
ECHO( 

CALL :printHeader, "Cleaning git"
git clean -ffdx
ECHO(

CALL :printHeader, "Pulling latest in git"
git pull
ECHO( 

CALL :printHeader, "Clean completed successfully"
goto :eof

:buildfailed
CALL :printError, "Clean failed"
goto :eof

:setESC
for /F "tokens=1,2 delims=#" %%a in ('"prompt #$H#$E# & echo on & for %%b in (1) do rem"') do (
  set ESC=%%b
  exit /B 0
)
exit /B 0

:removeFolder
    IF EXIST %~1 (
        ECHO Removing folder: %~1
        rd /s/q %~1
    ) ELSE (
        ECHO folder "%~1" does not exist!
    )
exit /b 0

:printHeader
REM //---------- PrintHeader function ----------
if not "%~1"=="" (
    ECHO %ESC%[33m*****************************************************************************************%ESC%[0m
    ECHO %ESC%[33m%~1%ESC%%[0
    ECHO %ESC%[33m*****************************************************************************************%ESC%%[0m
)
exit /b 0

:printError
REM //---------- PrintError function ----------
if not "%~1"=="" (
REM    ECHO %ESC%[31m*****************************************************************************************%ESC%[0m
    ECHO %ESC%[31m%~1%ESC%%[0
REM  ECHO %ESC%[31m*****************************************************************************************%ESC%%[0m
)
exit /b 0