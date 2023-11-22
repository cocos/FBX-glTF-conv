#Requires -Version "6.1"

param (
    [Parameter(Mandatory=$false)]
    [string]
    $ArtifactPath,

    [Parameter(Mandatory=$false)]
    [switch]
    $IncludeDebug = $False,

    [Parameter(Mandatory=$false)]
    [string]
    $Version = ""
)

$is64BitOperatingSystem = [System.Environment]::Is64BitOperatingSystem

Write-Host @"
IsWindows: $IsWindows
IsMacOS: $IsMacOS
IsLinux: $IsLinux
Is64BitOperatingSystem: $is64BitOperatingSystem
Current working directory: $(Get-Location)
ArtifactPath: $ArtifactPath
IncludeDebug: $IncludeDebug
Version: $Version
"@

function InstallVcpkg {
    git clone https://github.com/microsoft/vcpkg | Out-Host
    if ($IsWindows) {
        .\vcpkg\bootstrap-vcpkg.bat | Out-Host
    } elseif ($IsMacOS) {
        ./vcpkg/bootstrap-vcpkg.sh | Out-Host
        xcode-select --install | Out-Host
    } elseif ($IsLinux) {
        ./vcpkg/bootstrap-vcpkg.sh | Out-Host
    } else {
        Write-Error "vcpkg is not available on target platform."
    }
}

function GetTriplet {
    if ($IsWindows) {
        return "x64-windows"
    } elseif ($IsMacOS) {
        return "x64-osx"
    } elseif ($IsLinux) {
        return "x64-linux"
    } else {
        Write-Error "vcpkg is not available on target platform."
    }
}

function InstallFbxSdk {
    New-Item -ItemType Directory -Path "fbxsdk" | Out-Null
    if ($IsWindows) {
        $fbxSdkUrl = "https://www.autodesk.com/content/dam/autodesk/www/adn/fbx/2020-2-1/fbx202021_fbxsdk_vs2019_win.exe"
        $FbxSdkWindowsInstaller = Join-Path "fbxsdk" "fbxsdk.exe"
        (New-Object System.Net.WebClient).DownloadFile($fbxSdkUrl, $FbxSdkWindowsInstaller)
        $fbxSdkHome = [System.IO.Path]::Combine((Get-Location), "fbxsdk", "Home")
        Start-Process -Wait -FilePath $FbxSdkWindowsInstaller -ArgumentList "/S","/D=$fbxSdkHome"
    } elseif ($IsMacOS) {
        $fbxSdkUrl = "https://www.autodesk.com/content/dam/autodesk/www/adn/fbx/2020-2-1/fbx202021_fbxsdk_clang_mac.pkg.tgz"
        $fbxSdkVersion = "2020.2.1"
        $fbxSdkMacOSTarball = Join-Path "fbxsdk" "fbxsdk.pkg.tgz"
        (New-Object System.Net.WebClient).DownloadFile($fbxSdkUrl, $fbxSdkMacOSTarball)
        $fbxSdkMacOSPkgFileDir = "fbxsdk"
        & tar -zxvf $fbxSdkMacOSTarball -C $fbxSdkMacOSPkgFileDir | Out-Host
        $fbxSdkMacOSPkgFile = (Get-ChildItem -Path "$fbxSdkMacOSPkgFileDir/*" -Include "*.pkg").FullName
        Write-Host "FBX SDK MacOS pkg: $fbxSdkMacOSPkgFile"
        sudo installer -pkg $fbxSdkMacOSPkgFile -target / | Out-Host
        $fbxSdkHome = [System.IO.Path]::Combine((Get-Location), "fbxsdk", "Home")
        # Node gyp incorrectly handle spaces in path
        New-Item -ItemType SymbolicLink -Path "fbxsdk" -Name Home -Value "/Applications/Autodesk/FBX SDK/$fbxSdkVersion" | Out-Host
    } elseif ($IsLinux) {
        $fbxSdkUrl = "https://www.autodesk.com/content/dam/autodesk/www/adn/fbx/2020-2-1/fbx202021_fbxsdk_linux.tar.gz"
        $fbxSdkTarball = Join-Path "fbxsdk" "fbxsdk.tar.gz"
        Write-Host "Downloading FBX SDK tar ball from $fbxSdkUrl ..."
        (New-Object System.Net.WebClient).DownloadFile($fbxSdkUrl, $fbxSdkTarball)
        $fbxSdkTarballExtractDir = Join-Path "fbxsdk" "tarbar_extract"
        New-Item -ItemType Directory -Path $fbxSdkTarballExtractDir | Out-Null
        Write-Host "Extracting to $fbxSdkTarballExtractDir ..."
        & tar -zxvf $fbxSdkTarball -C $fbxSdkTarballExtractDir | Out-Host
        $fbxSdkInstallationProgram = Join-Path $fbxSdkTarballExtractDir "fbx202021_fbxsdk_linux"
        chmod ugo+x $fbxSdkInstallationProgram
        
        $fbxSdkHomeLocation = [System.IO.Path]::Combine($HOME, "fbxsdk", "install")
        Write-Host "Installing from $fbxSdkInstallationProgram..."
        New-Item -ItemType Directory -Path $fbxSdkHomeLocation | Out-Null

        # This is really a HACK way after many tries...
        "yes yes | $fbxSdkInstallationProgram $fbxSdkHomeLocation" | Out-File -File "./RunFbxsdkInstaller.sh"
        & bash "./RunFbxsdkInstaller.sh" | Out-Host
        Write-Host "`n"

        Write-Host "Installation finished($fbxSdkHomeLocation)."
        & ls $fbxSdkHomeLocation | Out-Host
        $fbxSdkHome = $fbxSdkHomeLocation
    } else {
        Write-Error "FBXSDK is not available on target platform."
    }
    return $fbxSdkHome
}

function InstallDependencies {
    ./vcpkg/vcpkg install
}

# https://stackoverflow.com/questions/54372601/running-git-clone-from-powershell-giving-errors-even-when-it-seems-to-work
$env:GIT_REDIRECT_STDERR = "2>&1"

$fbxSdkHome = InstallFbxSdk
Write-Host "FBX SDK: '$fbxSdkHome'"

InstallVcpkg

InstallDependencies

$cmakeInstallPrefix = "out/install"

$cmakeBuildTypes = @("Release")
if ($IncludeDebug) {
    $cmakeBuildTypes += "Debug"
}

foreach ($cmakeBuildType in $cmakeBuildTypes) {
    Write-Host "Build $cmakeBuildType ..."
    $cmakeBuildDir = "out/build/$cmakeBuildType"

    if ($IsWindows) {
        $polyfillsStdFileSystem = "OFF"
    } else {
        $polyfillsStdFileSystem = "ON"
    }

    $defineVersion = if ($Version) { "-DFBX_GLTF_CONV_CLI_VERSION=$Version" } else { "" }

    if ($IsMacOS) {
        cmake `
        -DCMAKE_TOOLCHAIN_FILE="vcpkg/scripts/buildsystems/vcpkg.cmake" `
        -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" `
        "-DCMAKE_BUILD_TYPE=$cmakeBuildType" `
        -DCMAKE_INSTALL_PREFIX="$cmakeInstallPrefix/$cmakeBuildType" `
        -DFbxSdkHome:STRING="$fbxSdkHome" `
        "-DPOLYFILLS_STD_FILESYSTEM=$polyfillsStdFileSystem" `
        "$defineVersion" `
        "-S." `
        "-B$cmakeBuildDir"
    }
    else {
        cmake `
        -DCMAKE_TOOLCHAIN_FILE="vcpkg/scripts/buildsystems/vcpkg.cmake" `
        "-DCMAKE_BUILD_TYPE=$cmakeBuildType" `
        -DCMAKE_INSTALL_PREFIX="$cmakeInstallPrefix/$cmakeBuildType" `
        -DFbxSdkHome:STRING="$fbxSdkHome" `
        "-DPOLYFILLS_STD_FILESYSTEM=$polyfillsStdFileSystem" `
        "$defineVersion" `
        "-S." `
        "-B$cmakeBuildDir"
    }

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

if ($ArtifactPath) {
    Compress-Archive -Path "$cmakeInstallPrefix\*" -DestinationPath $ArtifactPath
}
