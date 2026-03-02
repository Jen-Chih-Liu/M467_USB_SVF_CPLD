@echo off
REM 啟用延遲變數 (雖然在這個新版本中不是絕對必要，但保留是個好習慣)
SETLOCAL ENABLEDELAYEDEXPANSION

echo Cleaning Keil folders (preserving .bin files)...
echo.

REM --- 階段一：刪除 "整個" 要移除的資料夾 ---
echo Phase 1: Removing entire folders (lst, Listings, RTE)...
FOR /D /R . %%d IN (lst, Listings, RTE) DO (
    IF EXIST "%%d" (
        echo Removing "%%d"
        RMDIR /S /Q "%%d"
    )
)

echo.
REM --- 階段二：清理 "內部"，但保留 .bin 檔 ---
echo Phase 2: Cleaning 'obj'/'Objects' folders (keeping .bin)...
FOR /D /R . %%d IN (obj, Objects) DO (
    IF EXIST "%%d" (
        echo Cleaning inside "%%d"
        
        REM 暫時進入該資料夾
        PUSHD "%%d"
        
        REM 1. 刪除所有 "非 .bin" 檔案
        FOR %%f IN (*.*) DO (
            IF /I "%%~xf" NEQ ".bin" (
                DEL /Q /F "%%f"
            )
        )
        
        REM 2. D 刪除所有子資料夾
        FOR /D %%s IN (*) DO (
            RMDIR /S /Q "%%s"
        )
        
        REM 返回上一層目錄
        POPD
    )
)

ENDLOCAL
echo.
echo Done.
pause