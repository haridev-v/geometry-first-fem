@echo off
setlocal EnableExtensions DisableDelayedExpansion

rem Place this file beside the NUnit project and its .runsettings file.
rem NX_TEST_ENV_BAT can point to a site setup batch that sets variables and returns.
rem NX_TEST_PROJECT and NX_TEST_SETTINGS can override the project and settings paths.

if /i "%~1"=="--help" goto :usage
pushd "%~dp0" || exit /b 2
set "_nx_test_dir=%CD%"
set "_nx_test_exit=2"

if not defined NX_TEST_ENV_BAT goto :configure
if not exist "%NX_TEST_ENV_BAT%" (
    >&2 echo NX setup batch could not be found at "%NX_TEST_ENV_BAT%".
    goto :done
)
call "%NX_TEST_ENV_BAT%"
set "_nx_test_exit=%ERRORLEVEL%"
if not "%_nx_test_exit%"=="0" (
    >&2 echo NX setup batch failed with exit code %_nx_test_exit%.
    goto :done
)
set "_nx_test_exit=2"
cd /d "%_nx_test_dir%" || goto :done

:configure
rem For environment-based licensing, use values from a working NX session.
rem set "SPLM_LICENSE_SERVER=your-site-server-setting"
rem set "UGS_LICENSE_BUNDLE=your-site-bundle-setting"

if not defined NX_TEST_PROJECT set "NX_TEST_PROJECT=SimDataExtractor.IntegrationTests.csproj"
if not defined NX_TEST_SETTINGS set "NX_TEST_SETTINGS=mesh-topology.runsettings"
if not exist "%NX_TEST_PROJECT%" (
    >&2 echo NUnit project could not be found at "%NX_TEST_PROJECT%".
    goto :done
)
if not exist "%NX_TEST_SETTINGS%" (
    >&2 echo Runsettings file could not be found at "%NX_TEST_SETTINGS%".
    goto :done
)
where.exe dotnet.exe >nul 2>&1 || (
    >&2 echo Add the Windows x64 .NET SDK to PATH.
    goto :done
)

echo NX test launch environment
whoami
for %%V in (USERPROFILE UGII_BASE_DIR SPLM_LICENSE_SERVER UGS_LICENSE_BUNDLE) do (
    if defined %%V (
        set %%V
    ) else (
        echo %%V is unset.
    )
)
reg.exe query "HKCU\Software\Siemens_PLM_Software\Common_Licensing" /v NX_BUNDLES 2>nul
if errorlevel 1 echo NX_BUNDLES lookup returned no value.
echo(

if /i "%~1"=="--list-tests" goto :list_tests
set "_nx_test_filter="

:next_test
if "%~1"=="" goto :run_tests
if defined _nx_test_filter (
    set "_nx_test_filter=%_nx_test_filter%|FullyQualifiedName~%~1"
) else (
    set "_nx_test_filter=FullyQualifiedName~%~1"
)
shift /1
goto :next_test

:run_tests
if defined _nx_test_filter (
    dotnet.exe test "%NX_TEST_PROJECT%" --settings "%NX_TEST_SETTINGS%" --filter "%_nx_test_filter%" -- RunConfiguration.TreatNoTestsAsError=true
) else (
    dotnet.exe test "%NX_TEST_PROJECT%" --settings "%NX_TEST_SETTINGS%" -- RunConfiguration.TreatNoTestsAsError=true
)
set "_nx_test_exit=%ERRORLEVEL%"
goto :done

:list_tests
if not "%~2"=="" (
    >&2 echo Use --list-tests on its own.
    goto :done
)
dotnet.exe test "%NX_TEST_PROJECT%" --settings "%NX_TEST_SETTINGS%" --list-tests
set "_nx_test_exit=%ERRORLEVEL%"

:done
popd
endlocal & exit /b %_nx_test_exit%

:usage
echo Run all tests with run-nx-tests.bat
echo Select tests with run-nx-tests.bat BeamMeshMatchesInfoYaml AnotherTestName
echo Discover tests with run-nx-tests.bat --list-tests
exit /b 0
