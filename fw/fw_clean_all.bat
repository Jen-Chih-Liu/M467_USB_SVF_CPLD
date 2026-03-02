@echo off
echo Cleaning all Keil build folders recursively...

REM FOR /D /R .  -> 從目前目錄(.)開始，遞迴(/R)尋找所有目錄(/D)
REM %%d          -> 找到的目錄路徑
REM IN (obj, lst, Objects, Listings, RTE) -> 要尋找的資料夾名稱列表

FOR /D /R . %%d IN (obj, lst, Objects, Listings, RTE) DO (
    echo Removing "%%d"
    RMDIR /S /Q "%%d"
)

echo Done.
pause