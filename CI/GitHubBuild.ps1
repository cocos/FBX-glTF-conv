#Requires -Version "6.1"

param (
    [Parameter(Mandatory=$true)]
    [string]
    $ArtifactPath
)

$is64BitOperatingSystem = [System.Environment]::Is64BitOperatingSystem

Write-Host @"
IsWindows: $IsWindows
IsMacOS: $IsMacOS
Is64BitOperatingSystem: $is64BitOperatingSystem
Current working directory: $(Get-Location)
ArtifactPath: $ArtifactPath
"@

# New-Item -ItemType Directory -Path "artifacts"
# "Hello2!" | Out-File "artifacts/Hello.txt"
# Compress-Archive -Path "artifacts/*" -DestinationPath $ArtifactPath
# exit 0

function InstallVcpkg {
    git clone https://github.com/microsoft/vcpkg | Out-Host
    if ($IsWindows) {
        .\vcpkg\bootstrap-vcpkg.bat | Out-Host
    } elseif ($IsMacOS) {
        ./vcpkg/bootstrap-vcpkg.sh | Out-Host
        xcode-select --install | Out-Host
    } else {
        Write-Error "vcpkg is not available on target platform."
    }
}

function GetTriplet {
    if ($IsWindows) {
        return "x64-windows"
    } elseif ($IsMacOS) {
        return "x64-osx"
    } else {
        Write-Error "vcpkg is not available on target platform."
    }
}

function InstallFbxSdk {
    New-Item -ItemType Directory -Path "fbxsdk" | Out-Null
    if ($IsWindows) {
        $FBXSDK_2020_0_1_VS2017 = "https://damassets.autodesk.net/content/dam/autodesk/www/adn/fbx/2020-0-1/fbx202001_fbxsdk_vs2017_win.exe"
        $FbxSdkWindowsInstaller = Join-Path "fbxsdk" "fbxsdk.exe"
        (New-Object System.Net.WebClient).DownloadFile($FBXSDK_2020_0_1_VS2017, $FbxSdkWindowsInstaller)
        $fbxSdkHome = [System.IO.Path]::Combine((Get-Location), "fbxsdk", "Home")
        Start-Process -Wait -FilePath $FbxSdkWindowsInstaller -ArgumentList "/S","/D=$fbxSdkHome"
    } elseif ($IsMacOS) {
        $FBXSDK_2020_0_1_CLANG = "https://www.autodesk.com/content/dam/autodesk/www/adn/fbx/2020-0-1/fbx202001_fbxsdk_clang_mac.pkg.tgz"
        $FBXSDK_2020_0_1_CLANG_VERSION = "2020.0.1"
        $fbxSdkMacOSTarball = Join-Path "fbxsdk" "fbxsdk.pkg.tgz"
        (New-Object System.Net.WebClient).DownloadFile($FBXSDK_2020_0_1_CLANG, $fbxSdkMacOSTarball)
        $fbxSdkMacOSPkgFileDir = "fbxsdk"
        & tar -zxvf $fbxSdkMacOSTarball -C $fbxSdkMacOSPkgFileDir | Out-Host
        $fbxSdkMacOSPkgFile = (Get-ChildItem -Path "$fbxSdkMacOSPkgFileDir/*" -Include "*.pkg").FullName
        Write-Host "FBX SDK MacOS pkg: $fbxSdkMacOSPkgFile"
        sudo installer -pkg $fbxSdkMacOSPkgFile -target / | Out-Host
        $fbxSdkHome = [System.IO.Path]::Combine((Get-Location), "fbxsdk", "Home")
        # Node gyp incorrectly handle spaces in path
        New-Item -ItemType SymbolicLink -Path "fbxsdk" -Name Home -Value "/Applications/Autodesk/FBX SDK/$FBXSDK_2020_0_1_CLANG_VERSION" | Out-Host
    } else {
        Write-Error "FBXSDK is not available on target platform."
    }
    return $fbxSdkHome
}

function InstallDependencies {
    $triplet = GetTriplet

    ./vcpkg/vcpkg install
}

# https://stackoverflow.com/questions/54372601/running-git-clone-from-powershell-giving-errors-even-when-it-seems-to-work
$env:GIT_REDIRECT_STDERR = "2>&1"

InstallVcpkg

InstallDependencies

$fbxSdkHome = InstallFbxSdk
Write-Host "FBX SDK: $fbxSdkHome"

$cmakeInstallPrefix = "out/install"

foreach ($cmakeBuildType in @("Debug", "Release")) {
    Write-Host "Build $cmakeBuildType ..."
    $cmakeBuildDir = "out/build/$cmakeBuildType"

    if ($IsWindows) {
        $polyfillsStdFileSystem = "OFF"
    } else {
        $polyfillsStdFileSystem = "ON"
    }

    cmake `
    -DCMAKE_TOOLCHAIN_FILE="vcpkg/scripts/buildsystems/vcpkg.cmake" `
    "-DCMAKE_BUILD_TYPE=$cmakeBuildType" `
    -DCMAKE_INSTALL_PREFIX="$cmakeInstallPrefix/$cmakeBuildType" `
    -DFbxSdkHome:STRING="$fbxSdkHome" `
    "-DPOLYFILLS_STD_FILESYSTEM=$polyfillsStdFileSystem" `
    "-S." `
    "-B$cmakeBuildDir"

    # The build type really matters:
    # https://stackoverflow.com/questions/24460486/cmake-build-type-is-not-being-used-in-cmakelists-txt
    cmake --build $cmakeBuildDir --config $cmakeBuildType

    if ($IsWindows) {
        cmake --build $cmakeBuildDir --config $cmakeBuildType --target install
    } else {
        cmake --install $cmakeBuildDir
    }
}

if (((Test-Path $cmakeInstallPrefix) -eq $false) -or ((Get-Item $cmakeInstallPrefix) -isnot [System.IO.DirectoryInfo])) {
    Write-Error "Installed failed."
    exit -1
}

Compress-Archive -Path "$cmakeInstallPrefix\*" -DestinationPath $ArtifactPath
