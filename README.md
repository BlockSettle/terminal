# Introduction

The BlockSettle Terminal is an open source  Bitcoin wallet application which integrates private
key management with the ability to submit orders and execute
settlements in real-time. Terminal client implements a client-server architecture where
the user manages keys locally and have the choice of connecting
to a hosted headless ArmoryDB server.

## Features.

* Supports a built-in block explorer
* Support for Bech32 Address.(Bech32 is a new bitcoin address format specified by BIP 0173) 
* real time access to trading and real time XBT Market data service.
* Flexible security model for key management and safety.
* Free digital identity support using Auth eID.
  
## Concepts.
This section explains the concept needed to understand the operation of BlockSettle Terminal. And does not give in depth explanation of Bitcoins or Blockchain technology. If you run into any issues with the terminal client or you wish to get support on a certain functionality of the application or services, please login into to your BlockSettle client portal and submit a support ticket.

### Connecting to a Bitcoin Network.
The BlockSettle terminal performs  operations and transactions that are relevant to a Blockchain Wallet. To facilitate this, it connects to an infrastructure comprised of ArmoryDB server which is connected to a Bitcoin network. This software architecture design enable seamless management and safety of your Bitcoin wallet.

### Wallet Security and structure
The basic security model around Bitcoin is built on private and public
cryptographic keys. Private Keys are used to sign transactions. Public Keys are used
to receive balances.

When running BlockSettle Terminal for the first time you must make sure the private keys are safely backed up, but there are security implications of having one’s private keys
available on a machine connected to the internet. To mitigate these
risks, all BlockSettle wallets are “Watching-Only” and supported by an
encrypted signing container which can be located locally, remotely, or
offline. Users are therefore themselves able to determine the security
model around their private keys.

### XBT vs BTC.
"BTC" has been the generally accepted abbreviation for Bitcoin stemming from the early days of Bitcoin, but it was by public use and was not defined by any standard governing body.
"XBT" is a new abbreviation for Bitcoin that is starting to come into use after it was internationally standardized by International Standards Organization (ISO) that maintains a list of internationally recognized currencies.

## BlockSettle Terminal for Users.
This section is for those who are interested in running Terminal application for daily use as a wallet for non-custodial trading and settlement in Bitcoin. To get started users can easily connect to the BlockSettle.com hosted ArmoryDB Server, which supports Bitcoin "testnet" and "mainnet" supernodes. If you whish to connect to your own Armorydb Server, please refer to "BlockSettle Terminal for Hosting providers" section to create your own hosted node.

### Installing and running on Windows
The BlockSettle terminal is available for download for Microsoft Windows 7 and Above, which meets the following minimum system requirements. To get started visit our [Download](http://blocksettle.com/downloads/terminal) Page, and download the windows or macOS binary file that matches your operating system.

### Operating System Requirements.
- Linux - [Ubuntu](https://www.ubuntu.com/) (20.04 LTS/22.04 LTS)
- [macOS](https://www.apple.com/macos/) (10.12 or higher)
- [Windows](https://www.microsoft.com/en-us/windows) (Windows 10 or higher)

### Hardware Requirements.
* Dual Core CPU
* 1GB RAM
* 1GB Free disk space.
* A working internet connection.

After downloading bs-terminal_installer.exe, which should help you install the application on your system, and a shortcut should be available from the Windows start menu, after the installation completes.

### Installing and running on macOS
BlockSettle Terminal for macOS only supports macOS 10.12 and above, so make sure your system is running an up to date version of macOS, which meets this requirement. 

After you mount and run the download .dmg file, you can simply drag and drop the Terminal App to your macOS application folder and run the terminal application, like any other macOS app.

### Installing and Running on Linux.

For Linux, we maintain Ubuntu binary packages via launchpad.net which works on version 17.04 and above. Assuming you have set up snap package management on your system, you can execute the following setup of commands to easily install the application.

### Ubuntu 
To install the binary release on Ubuntu, 
```bash
sudo add-apt-repository ppa:blocksettle/bsterminal
sudo apt-get update

sudo apt-get install bsterminal
```
For more information and other available packages please visit our Launchpad page [launchpad.net/BlockSettle] (https://launchpad.net/~blocksettle/+archive/ubuntu/bsterminal )

## Become a Participant
The BlockSettle Terminal is a free-to-use and open source bitcoin wallet. Users who in addition wish to trade may register to become Participants.

Please visit [http://blocksettle.com/#registration](http://blocksettle.com/#registration) You will have to Download "Auth eID" app on your phone. Verify your identity in the App by uploading your passport and a recent address proof such as a utility bill or bank statement.

To create a BlockSettle account, enter the email address used in Auth eID and sign the registration within the app. Now you can login to the BlockSettle Terminal and start trading in our Private Market.

For access to trade our FX and XBT products please upgrade your account to Trading Participant within the Client Portal. Should you wish to provide responsive quotes in the XBT market, you can also apply for XBT dealing status.

## Developer Guide
### Supported runtime platforms
- Linux - [Ubuntu](https://www.ubuntu.com/) (16.04 LTS/18.04 LTS/18.10/19.04)
- [macOS](https://www.apple.com/macos/) (10.12 or higher)
- [Windows](https://www.microsoft.com/en-us/windows) (Windows 7 or higher)

**All OS versions must be 64-bit.**

### Supported compilers
- Linux: [GCC](https://gcc.gnu.org/)
- macOS: [Clang](https://clang.llvm.org/) (Command Line Tools for Xcode)
- Windows: [Visual Studio 2017](https://visualstudio.microsoft.com/) (Community Edition or higher)

## Platform-specific prerequisites
### Windows
 1. Download and install the Community version of [Visual Studio 2017](https://visualstudio.microsoft.com/downloads/), which is the minimum supported compiler. When prompted during installation, you need to download the C and C++ build tools, the Windows 8.1 SDK (`mpir` requirement), and the Windows Universal CRT SDK (`mpir` requirement).

 2. Install the following binaries.
- [Python](https://www.python.org/downloads/windows/). Download the latest 3.x version. (2.7 is still supported as of Mar. 2019 but support will be dropped eventually.) Using the Python 3.x from Visual Studio will also work.
- [cmake](https://cmake.org/download/). Select the win64-x64 installer.
- [MySQL Connector](https://dev.mysql.com/downloads/windows/installer/). The smaller, web-based installer is acceptable but either installer is acceptable. In the installer, install both the "Connector/C" and "Connector/C++" options. No other options are required.
- [YASM](https://yasm.tortall.net/Download.html). Download the Win64 VS2010 ZIP archive and unpack it to `C:\Program Files\yasm`.
- [Strawberry Perl](http://strawberryperl.com/). 64 bit version. Restart the command prompt for the changes to PATH to take effect.
- [NASM](https://www.nasm.us/). Select the latest stable version, go into the `win64` directory, and download the installer.

 3. Add `%PYTHON_HOME%` and `%PYTHON_HOME%\Scripts` (Python variables, if you are using the Visual Studio version this is unnecessary) and `C:\Program Files\NASM` (the NASM directory) to the Windows `PATH` environment variable. (See "Environment Variables" under the "Advanced" tab in System Properties.)

 4. Check your home directory for spaces, which aren't allowed. For example, `C:\Satoshi Nakamoto` won't work. (`C:\Satoshi` would be okay.) If your home directory has a space in it, add the `DEV_3RD_ROOT` environment variable to Windows, as seen in the ["Terminal prerequisites"](#terminal-prerequisites) section.

 5. Click the Start button and select the `x64 Native Tools Command Prompt for VS 2022` program. You may have to type the name until the option appears. It is *critical* that you type `x64` and *not* `x86`.

## Ubuntu
 Execute the following commands:

	sudo apt install python-pip cmake libmysqlclient-dev autoconf libtool yasm nasm libgmp3-dev libdouble-conversion-dev
	sudo apt install qttools5-dev-tools libfreetype-dev libfontconfig-dev libcups2-dev xcb libudev-dev libxi-dev libsm-dev libxrender-dev libdbus-1-dev
	sudo apt install libx11-xcb-dev libxcb-xkb-dev libxcb-xinput-dev libxcb-sync-dev libxcb-render-util0-dev libxcb-xfixes0-dev libxcb-xinerama0-dev libxcb-randr0-dev libxcb-image0-dev libxcb-keysyms1-dev libxcb-icccm4-dev libxcb-glx0-dev libxkbcommon-x11-dev

### MacOS
 1. Get an Apple developer account (free), log in, and download the latest version of `Command Line Tools for Xcode`. As an alternative, install the latest version of [Xcode](https://itunes.apple.com/us/app/xcode/id497799835) and download `Command Line Tools` via Xcode. Either choice will be updated via the App Store.

 2. Install and update [Homebrew](https://brew.sh). Warnings can probably be ignored, although environment differences and changes Apple makes to the OS between major releases make it impossible to provide definitive guidance.

		/usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
		touch ~/.bashrc
		echo "export CFLAGS=\"-arch x86_64\"" >> ~/.bashrc
		echo "export ARCHFLAGS=\"-arch x86_64\"" >> ~/.bashrc
		source ~/.bashrc
		brew update
		brew doctor

 3. Install and link dependencies required by the BlockSettle terminal build process but not by the final binaries.

		brew install python3 cmake mysql-connector-c mysql-connector-c++ automake libtool yasm nasm xz pkg-config
		echo 'export PATH="/usr/local/opt/mysql-client/bin:$PATH"' >> ~/.bash_profile
		echo 'export PATH="/usr/local/opt/qt/bin:$PATH"' >> ~/.bash_profile
		source ~/.bash_profile

 4. Reset the build machine. (This is necessary due to issues related to the Python install.)

 5. Create a symbolic link for glibtoolize. This requires sudo access and is not strictly necessary. It makes Autotools much happier, though, and should be harmless otherwise.

		sudo ln -s /usr/local/bin/glibtoolize /usr/local/bin/libtoolize

### Build instructions for all platforms
 1. Use `pip` to install the `wget` and `requests` Python packages. Note that, on Linux and macOS, the `pip` binary might actually need to be the `pip3` binary if Python 2.7 and 3.x are both on the system.

		pip install wget requests pathlib

 2. Clone the BlockSettle Terminal repo. If asked for your GitHub user/pass, get in touch with the devs and confirm that you have proper access to the repos (`terminal` and the submodules). If you continue to have access issues, confirm whether or not you use 2FA. If you do, you need to use a [Personal Access Token](https://help.github.com/articles/creating-a-personal-access-token-for-the-command-line/) as your password. The PAT should be separate from any other PATs in case the build machine is compromised; the PAT can be revoked without breaking uncompromised PATs.

 3. If necessary, set your branch to the appropriate branch. The following example uses the `bs_dev` branch, and the command must be issued in the source code's root directory.

		git checkout bs_dev

 4. Initialize the Git submodules. You may be asked for your password again. If so, use your password/PAT.

		git config credential.helper store  (NB: This command saves the password/PAT for the submodules, and need only be issued once in the project's lifetime. Once successful, don't use it again.)
		git submodule update --init --recursive

 5. Build the terminal compilation environment. This will involve downloading and compiling all required prerequisite binaries for the terminal, and performing any required environment changes. Note that on Linux and macOS, the `python` binary may actually be `python2`.

 6. By default, the terminal is built in release mode. The `debug` option is required only when attempting to create a build environment for debug builds. Don't use debug builds unless you're planning to run the code via a debugger. An example is found below.

		python generate.py debug

 7. By default, the terminal is built as a static binary. Building it as a shared binary is also an option. The `shared` option must be used after the release/debug mode. An example is found below.

		python generate.py release shared

 8. From the BlockSettle Terminal root directory, go to the `terminal.release` or `terminal.debug` subdirectory (`terminal.release-shared` or `terminal.debug-shared` if running a shared build), depending on if the `debug` argument was used when generating the environment. On Linux or macOS, execute `make` as one would for any other project. On Windows, open the `BS_Terminal.sln` file in Visual Studio and select `Build -> Build Solution` from the menu bar, or press `CTRL+SHIFT+B`. No matter which platform you use, you hopefully won't get any compiler or linker errors. (If you do, please check if this is a known issue. If not, and you have the capability, please submit a PR that properly fixes the problem.)

 9. Assuming the code compiles and links successfully, from the BlockSettle Terminal root directory, go to the `build_terminal` subdirectory. From there, go to the `Release/bin` or `Debug/bin` subdirectories to find the generated binaries. Linux and Windows binaries may be executed directly. On macOS, the user will either run the `blocksettle_signer` binary or double-click the BlockSettle Terminal app (or type `open -a Blocksettle\ Terminal` from the command line), or both if in remote mode. If the macOS version needs to have command line arguments passed, the easiest thing to do is to go directly to the actual executable. The binary to execute will be in the `BlockSettle Terminal.app/Contents/MacOS/` subdirectory.

 10. When pulling code, the submodules may be updated, or new submodules may be added. Run the following commands to ensure that the submodules are properly updated.

		git submodule init  (Required *only* if a new submodule has been added.)
		git submodule update --init --recursive

 11. (**WINDOWS ONLY**) A one-time step is required upon the first compilation. Go to `DEV_3RD_ROOT/release/ZeroMQ/lib` and copy the libzmq DLL file to same directory as the BlockSettle Terminal binary. The DLL need only be copied once but it'll have to be re-copied whenever `libzmq` is updated. This will eventually be automated. For now, the build script occasionally crashes when attempting to automate this process.

# Miscellaneous
## Terminal prerequisites
The terminal does need to download and compile some prerequisites in order for the terminal to run. As of Nov. 2018, these prerequisites are:

- [Botan](https://botan.randombit.net/)
- [Google Test](https://github.com/abseil/googletest)  (Required only if test tools are built)
- [Jom](https://wiki.qt.io/Jom)  (Required only by Windows)
- [libbtc](https://github.com/libbtc/libbtc)
- [libchacha20poly1305](https://github.com/jonasschnelli/chacha20poly1305/)
- [mpir](http://mpir.org/)
- [OpenSSL](https://www.openssl.org/)
- [Protocol Buffers](https://developers.google.com/protocol-buffers/)
- [Qt](https://www.qt.io/)
- [spdlog](https://github.com/gabime/spdlog)
- [SQLite](https://www.sqlite.org/)
- [WebSockets](https://libwebsockets.org/)
- [ZeroMQ (libzmq)](http://zeromq.org/)

By default, the prerequisites are downloaded and installed in the `3rd` subdirectory in the `terminal` base directory. This may not always be feasible for various reasons. If necessary, set the `DEV\_3RD\_ROOT` variable in the OS environment in order to change this location. The directions for each OS are:

- Linux (Ubuntu) - In ~/.bashrc, export the variable to the desired location. (Example: `export DEV_3RD_ROOT="$HOME/Projects/BlockSettle-alt-location/3rd"`)
- Windows - Add the `DEV_3RD_ROOT` variable to the Windows system environment, similar to the manner in which the `PATH` variable was accessed in the [Windows platform-specific prerequisites](#windows) section.
- macOS - In ~/.bash\_profile, export the variable to the desired location. (Example: `export DEV_3RD_ROOT="$HOME/Projects/BlockSettle-alt-location/3rd"`)

## Troubleshooting
The mpir library may fail to build on certain CPUs (e.g., Intel's 7xxx series). It's possible to apply a workaround when using a VMWare VM. Change the "Virtualize Intel VT-X/EPT or AMD-V/RVI" checkbox in the VM configuration GUI. In addition, add the following lines to the appropriate virtual machine configuration (.vmx file):

     cpuid.1.edx = "10111111111010111111101111111011"
     cpuid.1.eax = "00000000000000110100011010101001"

