@echo off
REM ESP-IDF Environment Setup Batch File
REM This batch file calls the PowerShell script to set up ESP-IDF environment

echo Setting up ESP-IDF environment...
powershell.exe -ExecutionPolicy Bypass -File "%~dp0setup-esp-idf.ps1"