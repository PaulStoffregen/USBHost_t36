@echo off
SETLOCAL DisableDelayedExpansion EnableExtensions
title TSET Arduino CMD line build
rem *******************************
rem Frank Bösing 11/2018
rem Windows Batch to compile Arduino sketches

rem Usage:
rem compile.cmd 0 : compile sketch
rem compile.cmd 1 : compile & upload sketch
rem compile.cmd 2 : rebuild & upload sketch
rem - Attention: Place compile.cmd in Sketch folder!
rem
rem Edit these paths:

set arduino=C:\arduino-1.8.19
rem set TyTools=C:\Program Files\TyQt
set TyTools=D:\GitHub\tytools\build\win64\Release
set libs=C:\Users\kurte\Documents\Arduino\libraries
set tools=D:\GitHub\Tset
REM Pick a TEMP folder IF not the Windows Default folder - remove REM
REM set TsetTemp=t:\temp

rem *******************************
rem Set Teensy-specific variables here:
rem


REM defragster was here 

set model=teensyMM
set speed=600
set opt=o2std
set usb=serial
cd.
set sketchcmd=HIDDeviceInfo.ino

rem set keys=de-de
set keys=en-us

rem *******************************
rem Don't edit below this line
rem *******************************

if EXIST %sketchcmd% (
  set sketchname=%sketchcmd%
) ELSE for %%i in (*.ino) do set sketchname=%%i

if "%sketchname%"=="" (
  echo No Arduino Sketch found!
  exit 1
)

set myfolder=.\
set ino="%myfolder%%sketchname%"
if "x%TsetTemp%"=="%TsetTemp%x" set TsetTemp=%temp%
set temp1="%TsetTemp%\\arduino_build_%sketchname%"
set temp2="%TsetTemp%\\arduino_cache_%sketchname%"
set fqbn=teensy:avr:%model%:usb=%usb%,speed=%speed%,opt=%opt%,keys=%keys%

if "%model%"=="teensyMM" set model=TEENSY_MICROMOD
rem Comment line below to build prior to TeensyDuino 1.50
if "%model%"=="teensy31" set model=teensy32

if "%1"=="2" (
  echo Temp: %temp1%
  echo Temp: %temp2%
  del /s /q %temp1%>NUL
  del /s /q %temp2%>NUL
  echo Temporary files deleted.
)

if not exist %temp1% mkdir %temp1%
if not exist %temp2% mkdir %temp2%

REM if not exist %temp1%\pch mkdir %temp1%\pch
REM if exist userConfig.h copy userConfig.h %temp1%\pch

if exist %arduino%\portable\sketchbook\libraries\.  set libs=%arduino%\portable\sketchbook\libraries
if exist %arduino%\portable\sketchbook\libraries\.  echo Building PORTABLE: %libs% 

echo Building Sketch: %ino%
"%arduino%\arduino-builder" -verbose=1 -warnings=more -compile -logger=human -hardware "%arduino%\hardware" -hardware "%LOCALAPPDATA%\Arduino15\packages" -tools "%arduino%\tools-builder" -tools "%arduino%\hardware\tools\avr" -tools "%LOCALAPPDATA%\Arduino15\packages" -built-in-libraries "%arduino%\libraries" -libraries "%libs%" -fqbn=%fqbn% -build-path %temp1% -build-cache "%temp2%"  %ino%

if not "%1"=="0" (
	REM Use TyComm with IDE to reboot for TeensyLoader Update // tycmd reset -b
  if "%errorlevel%"=="0" (
      "%TyTools%\TyCommanderC.exe" upload --autostart --wait --delegate "%temp1%\%sketchname%.hex" )
)

if "%1x"=="x%1" PAUSE
if not "%1x"=="x%1" exit %errorlevel%


rem "T:\arduino-1.8.12\hardware\tools\arm\bin\arm-none-eabi-gdb.exe" "T:\TEMP\arduino_build_breakpoint_test.ino\breakpoint_test.ino.elf"
rem "T:\arduino-1.8.12\hardware\tools\arm\bin\arm-none-eabi-gdb-py.exe"
REM   "%arduino%\hardware\tools\arm\bin\arm-none-eabi-gdb.exe" "%temp1%\%sketchname%.%model%.elf"

rem  "%tools%\GDB.cmd" "%arduino%\hardware\tools\arm\bin\arm-none-eabi-gdb-py.exe" "%temp1%\%sketchname%.elf" --tui
rem (gdb) target remote \\.\com21