:: Utils

:: MICROSERVER EXE LOCATION (Debug for now, Release seems to not work)
set "SERVER_EXE=C:\gitbuilds\OMNI-BIC-DEVELOPMENT\gRPC\BICgRPCServer\cmake\build\Debug\BICgRPCmicroserver.exe"

:: SERVER IP ADDRESS
set SERVER_IP=127.0.0.1

:: SERVER PORT
set SERVER_PORT=50051

:: Jump to label
goto %1

:START_SERVER
if not exist "%~2" mkdir "%~2"
start "BIC gRPC Server" "%SERVER_EXE%" "%~2" "%~3"
goto SERVER_WAIT_LOOP

:SERVER_WAIT_LOOP
powershell -NoProfile -Command ^
    "if ((Test-NetConnection 127.0.0.1 -Port 50051 -WarningAction SilentlyContinue).TcpTestSucceeded) { exit 0 } else { exit 1 }"
if errorlevel 1 (
    timeout /t 1 /nobreak >nul
    goto SERVER_WAIT_LOOP
)
:goto eof

:PERSERVE_CONFIGURATION
for %%F in ("%~2") do (
    copy /Y "%%F" "%LOG_DIRECTORY%\%%~nF_%TIMESTAMP%%%~xF" >nul
)
