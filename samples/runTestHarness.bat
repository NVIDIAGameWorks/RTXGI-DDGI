::
:: Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
::
:: NVIDIA CORPORATION and its licensors retain all intellectual property
:: and proprietary rights in and to this software, related documentation
:: and any modifications thereto.  Any use, reproduction, disclosure or
:: distribution of this software and related documentation without an express
:: license agreement from NVIDIA CORPORATION is strictly prohibited.
::

@echo off
echo.
cd /d build\test-harness
echo Launching Test Harness...

:: Cornell Box
::..\bin\Debug\TestHarness.exe ..\..\test-harness\config\cornell.ini
..\bin\Release\TestHarness.exe ..\..\test-harness\config\cornell.ini

:: Two rooms
::..\bin\Debug\TestHarness.exe ....\test-harness\config\two-rooms.ini
::..\bin\Release\TestHarness.exe ..\..\test-harness\config\two-rooms.ini

echo Done.
cd /d ..\..\
