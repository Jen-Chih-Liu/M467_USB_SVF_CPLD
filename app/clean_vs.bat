@echo off
CHCP 65001 > nul

REM 變更目前工作目錄到批次檔所在的目錄
CD /D "%~dp0"

echo.
echo =================================================================
echo                      Visual Studio 快速深度清理腳本
echo =================================================================
echo.
echo 將會刪除「目前目錄 (%CD%)」及其所有子目錄中:
echo 1. 「.vs」資料夾
echo 2. *.exp, *.pdb, *.tlog, *.recipe, *.ilk, *.log, *.obj, *.idb
echo.
echo !!! 警告: 這是一個「永久刪除」操作，無法復原 !!!
echo.
echo 按下任意鍵開始執行 (或按 Ctrl+C 取消)...
pause > nul
echo.

REM --- 1. 刪除 .vs 資料夾 ---
echo 正在刪除 .vs 資料夾...
IF EXIST ".vs" (
    REM /S = 刪除所有子目錄和檔案, /Q = 安靜模式
    RD /S /Q ".vs"
)

REM --- 2. 刪除中繼檔案 ---
echo 正在快速刪除中繼檔案...
REM /S = 遞迴, /Q = 安靜, /F = 強制
DEL /S /Q /F *.exp *.pdb *.tlog *.recipe *.ilk *.log *.obj *.idb

echo.
echo =================================================================
echo                      清理完成！
echo =================================================================
echo.
pause