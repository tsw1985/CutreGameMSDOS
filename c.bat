@echo off

echo Copying project...
echo To K:\CutreGameMSDOS
echo From C:\code\tanks
echo.

xcopy C:\code\tanks\ K:\CutreGameMSDOS\*.*  /E /R /Y /I

if errorlevel 1 goto error

echo.
echo Copy done correctly
goto end

:error
echo.
echo ERROR: Can not copy all files

:end
pause