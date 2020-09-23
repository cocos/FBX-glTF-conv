

enum Platform {
    windows
    macOS
}

if (($env:APPVEYOR_BUILD_WORKER_IMAGE).StartsWith('Visual Studio')) {
    $platform = [Platform]::windows
} else {
    $platform = [Platform]::macOS
}

function InstallVcpkg {
    if ($platform -eq [Platform]::windows) {
        git clone https://github.com/microsoft/vcpkg | Out-Host
        .\vcpkg\bootstrap-vcpkg.bat | Out-Host
    } elseif ($platform -eq [Platform]::macOS) {
        git clone https://github.com/microsoft/vcpkg | Out-Host
        ./vcpkg/bootstrap-vcpkg.sh | Out-Host
        xcode-select --install | Out-Host
    } else {
        Write-Error "vcpkg is not available on target platform."
    }
}

function GetTriplet {
    if ($platform -eq [Platform]::windows) {
        return "x64-windows"
    } elseif ($platform -eq [Platform]::macOS) {
        return "x64-osx"
    } else {
        Write-Error "vcpkg is not available on target platform."
    }
}

function InstallFbxSdk {
    New-Item -ItemType Directory -Path "fbxsdk" | Out-Null
    if ($platform -eq [Platform]::windows) {
        $FBXSDK_2020_0_1_VS2017 = "https://damassets.autodesk.net/content/dam/autodesk/www/adn/fbx/2020-0-1/fbx202001_fbxsdk_vs2017_win.exe"
        $FbxSdkWindowsInstaller = Join-Path "fbxsdk" "fbxsdk.exe"
        Start-FileDownload $FBXSDK_2020_0_1_VS2017 $FbxSdkWindowsInstaller
        $fbxSdkHome = [System.IO.Path]::Combine((Get-Location), "fbxsdk", "Home")
        Start-Process -Wait -FilePath $FbxSdkWindowsInstaller -ArgumentList "/S","/D=$fbxSdkHome"
    } elseif ($platform -eq [Platform]::macOS) {
        $FBXSDK_2020_0_1_CLANG = "https://www.autodesk.com/content/dam/autodesk/www/adn/fbx/2020-0-1/fbx202001_fbxsdk_clang_mac.pkg.tgz"
        $FBXSDK_2020_0_1_CLANG_VERSION = "2020.0.1"
        $fbxSdkMacOSTarball = Join-Path "fbxsdk" "fbxsdk.pkg.tgz"
        Start-FileDownload $FBXSDK_2020_0_1_CLANG $fbxSdkMacOSTarball
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

    ./vcpkg/vcpkg install libxml2:$triplet zlib:$triplet nlohmann-json:$triplet fmt:$triplet clipp:$triplet cppcodec:$triplet range-v3:$triplet
}

# https://stackoverflow.com/questions/54372601/running-git-clone-from-powershell-giving-errors-even-when-it-seems-to-work
$env:GIT_REDIRECT_STDERR = "2>&1"

InstallVcpkg

InstallDependencies

$fbxSdkHome = InstallFbxSdk
Write-Host "FBX SDK: $fbxSdkHome"

$cmakeInstallPrefix = "out/install"

$archName = ($env:PLATFORM).ToLower()
if ($platform -eq [Platform]::windows) {
    $osName = "win32"
} elseif ($platform -eq [Platform]::macOS) {
    $osName = "darwin"
} else {
    $osName = "unknown"
}

foreach ($cmakeBuildType in @("Debug", "Release")) {
    Write-Host "Build $cmakeBuildType ..."
    $cmakeBuildDir = "out/build/$cmakeBuildType"

    if ($platform -eq [Platform]::windows) {
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

    cmake --build $cmakeBuildDir

    if ($platform -eq [Platform]::windows) {
        cmake --build $cmakeBuildDir --target install
    } else {
        cmake --install $cmakeBuildDir
    }
}

if (((Test-Path $cmakeInstallPrefix) -eq $false) -or ((Get-Item $cmakeInstallPrefix) -isnot [System.IO.DirectoryInfo])) {
    Write-Error "Installed failed."
    exit -1
}

Tree $cmakeInstallPrefix /F

$artifactPackageName = "FBX-glTF-conv-$($env:APPVEYOR_BUILD_VERSION)-$osName-$archName.zip"
Write-Host "Artifact package name: $artifactPackageName"
Compress-Archive -Path "$cmakeInstallPrefix\*" -DestinationPath $artifactPackageName
Push-AppveyorArtifact $artifactPackageName
