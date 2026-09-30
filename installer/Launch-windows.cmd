@echo off
cd /d "%~dp0"
where py >nul 2>nul
if errorlevel 1 goto try_python
py -3 -c "import sys, tkinter; assert sys.version_info >= (3, 10)" >nul 2>nul
if errorlevel 1 goto try_python
set task_python=py -3
goto launch
:try_python
where python >nul 2>nul
if errorlevel 1 goto missing_python
python -c "import sys, tkinter; assert sys.version_info >= (3, 10)" >nul 2>nul
if errorlevel 1 goto missing_python
set task_python=python
:launch
%task_python% wizard.py
if errorlevel 1 pause
exit /b
:missing_python
echo This preview needs Python 3.10 or newer with Tk. No files were changed.
pause
exit /b 1
