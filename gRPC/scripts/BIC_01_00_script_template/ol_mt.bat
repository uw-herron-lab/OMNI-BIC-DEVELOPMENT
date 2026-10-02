@echo off
setlocal

:: Generate date
for /f %%I in ('powershell -NoProfile -Command "Get-Date -Format yyyy-MM-dd"') do set "TODAY=%%I"

:: =============================================================
:: Configuration
:: =============================================================
set "SERVER_EXE=C:\gitbuilds\OMNI-BIC-DEVELOPMENT\gRPC\BICgRPCServer\cmake\build\Release\BICgRPCmicroserver.exe"
:: =============================================================
set "APP_EXE=C:\gitbuilds\OMNI-BIC-DEVELOPMENT\gRPC\Client Examples\MotorEvokedPotentialsApp\bin\Release\MotorEvokedPotentialsApp.exe"
:: =============================================================
set "LOG_DIRECTORY=C:\BICData\BIC_01_00\%TODAY%\ol_mt"
:: =============================================================

:: Check whether a gRPC server is already running
powershell -NoProfile -Command ^
    "$client = New-Object System.Net.Sockets.TcpClient; try { $client.Connect('127.0.0.1', 50051); exit 0 } catch { exit 1 } finally { $client.Dispose() }"
if not errorlevel 1 (
    echo.
    echo ERROR
    echo ================================================
    echo A gRPC server is already listening on port 50051
    echo Close any existing gRPC servers before running
    echo ================================================
    echo.
    pause
    endlocal
    exit /b 1
)

:: Launch gRPC server and wait until ready
if not exist "%LOG_DIRECTORY%" mkdir "%LOG_DIRECTORY%"
start "BIC gRPC Server" "%SERVER_EXE%" "%LOG_DIRECTORY%" "%LOG_DIRECTORY%"
:WAIT_FOR_SERVER
powershell -NoProfile -Command ^
    "if ((Test-NetConnection 127.0.0.1 -Port 50051 -WarningAction SilentlyContinue).TcpTestSucceeded) { exit 0 } else { exit 1 }"
if errorlevel 1 (
    timeout /t 1 /nobreak >nul
    goto WAIT_FOR_SERVER
)

:: Launch application and pass configurations
start "" "%APP_EXE%" "%LOG_DIRECTORY%"

endlocal
exit /b 0