@echo off
cd /d %~dp0

for /F "tokens=2,3" %%i in (..\include\m_version.h) do if "%%i"=="MIRANDA_VERSION_FILEVERSION" (set OldVer=%%j)
echo %OldVer%

for /F "tokens=1,2" %%i in ('svn info miranda32.sln') do call :AddBuild %%i %%j
goto :eof

:AddBuild
if (%1) == (Revision:) (
	for /F "tokens=1,2,3 delims= " %%i in (build.no) do call :WriteVer %%i %%j %%k %2
   )

goto :eof

:WriteVer
if "%OldVer%" == "%1,%2,%3,%4" (goto :eof)

for /f "delims=/ tokens=1-3" %%a in ("%DATE:~4%") do (
	for /f "delims=:. tokens=1-4" %%m in ("%TIME: =0%") do (
		set TempFileName=%TEMP%\basename-%%c-%%b-%%a-%%m%%n%%o%%p
	)
)

copy m_version.h.in "%TempFileName%"

echo #define MIRANDA_VERSION_FILEVERSION %1,%2,%3,%4                               >> "%TempFileName%"
echo #define MIRANDA_VERSION_STRING      "%1.%2.%3.%4"                             >> "%TempFileName%"
echo #define MIRANDA_VERSION_DISPLAY     "%1.%2.%3 alpha build #%4"                >> "%TempFileName%"
echo #define MIRANDA_VERSION_DWORD       MIRANDA_MAKE_VERSION(%1, %2, %3, %4)      >> "%TempFileName%"
echo #define MIRANDA_VERSION_CORE        MIRANDA_MAKE_VERSION(%1, %2, %3, 0)       >> "%TempFileName%"
echo #define MIRANDA_VERSION_CORE_STRING "%1.%2.%3.0"                              >> "%TempFileName%"
echo.                                                                              >> "%TempFileName%"
echo #endif // M_VERSION_H__                                                       >> "%TempFileName%"

move /Y "%TempFileName%" ..\include\m_version.h
goto :eof
