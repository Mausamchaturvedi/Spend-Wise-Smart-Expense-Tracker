@echo off
"C:\msys64\ucrt64\bin\g++.exe" -std=c++17 -O0 src\server.cpp -o spendwise.exe -lws2_32
echo.
echo COMPILER EXIT CODE: %ERRORLEVEL%
pause
