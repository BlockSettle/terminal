# Supported runtime platforms
- Linux ([Ubuntu](https://www.ubuntu.com/) 16.04 LTS/18.04 LTS/18.10)
- [macOS](https://www.apple.com/macos/) (10.12 or higher)
- [Windows](https://www.microsoft.com/en-us/windows) (Windows 7 or higher)

**All OS versions must be 64-bit.**

# Supported compilers
- Linux: [GCC](https://gcc.gnu.org/)
- macOS: [Clang](https://clang.llvm.org/) (Command Line Tools for Xcode)
- Windows: [Visual Studio 2017](https://visualstudio.microsoft.com/) (Community Edition or higher)

# Platform-specific prerequisites
## Windows
 1. Download and install the Community version of [Visual Studio 2017](https://visualstudio.microsoft.com/downloads/), which is the minimum supported compiler. When prompted during installation, you will only need the C and C++ build tools.

 2. Install the following binaries.
- [Python](https://www.python.org/downloads/windows/). Download the latest 2.7 version. (This will eventually change to the latest 3.x version.)
- [cmake](https://cmake.org/download/). Select the win64-x64 installer.
- [MySQL Connector](https://dev.mysql.com/downloads/windows/installer/). The smaller, web-based installer is acceptable but either installer is acceptable. In the installer, install both the "Connector/C" and "Connector/C++" options. No other options are required.
- [YASM](https://yasm.tortall.net/Download.html). Download the Win64 VS2010 ZIP archive and unpack it to `C:\Program Files\yasm`.
- [ActiveState Perl](https://www.activestate.com/products/activeperl/downloads/).
- [NASM](https://www.nasm.us/). Select the latest stable version, go into the `win64` directory, and download the installer.

 3. Add `%PYTHON_HOME%` and `%PYTHON_HOME%\Scripts` (Python variables) and `C:\Program Files\NASM` (the NASM directory) to the Windows `PATH` environment variable. (See "Environment Variables" under the "Advanced" tab in System Properties.)

 4. Check your home directory for spaces, which aren't allowed. For example, `C:\Satoshi Nakamoto` won't work. (`C:\Satoshi` would be okay.) If your home directory has a space in it, add the `DEV_3RD_ROOT` environment variable to Windows, as seen in the ["Terminal prerequisites"](#terminal-prerequisites) section.

 5. Click the Start button and select the `x64 Native Tools Command Prompt for VS 2017` program. You may have to type the name until the option appears. It is *critical* that you type `x64` and *not* `x86`.

## Ubuntu
 1. Open Software Updates -> Ubuntu Software -> set "Source Code" checkbox (required for qt5-default).

 2. Execute the following commands.

		sudo apt install python-pip cmake libmysqlclient-dev autoconf libtool yasm nasm g++
		sudo apt build-dep qt5-default

## MacOS
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

		brew install python2 cmake mysql-connector-c mysql-connector-c++ automake libtool yasm nasm xz pkg-config
		echo 'export PATH="/usr/local/opt/mysql-client/bin:$PATH"' >> ~/.bash_profile
		echo 'export PATH="/usr/local/opt/qt/bin:$PATH"' >> ~/.bash_profile
		source ~/.bash_profile

 4. Reset the build machine. (This is necessary due to issues related to the Python install.)

 5. Create a symbolic link for glibtoolize. This requires sudo access and is probably not strictly necessary. It makes Autotools much happier, though, and should be harmless otherwise.

		sudo ln -s /usr/local/bin/glibtoolize /usr/local/bin/libtoolize
		
 6. MPIR library may fail to build on latest CPU's like Intel 7xxx. It's possible to apply workaround in case of VMWare VM used. Add to virtual machine configuration (.vmx file) following strings:
 		
		cpuid.1.edx = "10111111111010111111101111111011"
		cpuid.1.eax = "00000000000000110100011010101001"
Also try to set/unset "Virtualize Intel VT-X/EPT or AMD-V/RVI" checkbox in virtual machine configuration.

# Build instructions for all platforms
 1. Use `pip` to install the `wget` and `requests` Python packages. Note that, on Linux and macOS, the `pip` binary might actually need to be the `pip2` binary as long as Python 2.7 is being used to run the build script.

		pip install wget requests

 2. Clone the BlockSettle Terminal repo. If asked for your GitHub user/pass, get in touch with the devs and confirm that you have proper access to the repos (`terminal` and the submodules). If you continue to have access issues, confirm whether or not you use 2FA. If you do, you need to use a [Personal Access Token](https://help.github.com/articles/creating-a-personal-access-token-for-the-command-line/) as your password. The PAT should be separate from any other PATs in case the build machine is compromised; the PAT can be revoked without breaking uncompromised PATs.

 3. If necessary, set your branch to the appropriate branch. The following example uses the `bs_dev` branch, and the command must be issued in the source code's root directory.

		git checkout bs_dev

 4. Initialize the Git submodules. You may be asked for your password again. If so, use your password/PAT.

		git config credential.helper store  (NB: This command saves the password/PAT for the submodules, and need only be issued once in the project's lifetime. Once successful, don't use it again.)
		git submodule init
		git submodult update

 5. Build the terminal compilation environment. This will involve downloading and compiling all required prerequisite binaries for the terminal, and performing any required environment changes. Note that on Linux and macOS, the `python` binary may actually be `python2`. In addition, the `debug` option is required only when attempting to create a build environment for debug builds. Don't use debug builds unless you're planning to run the code via a debugger, and don't type the brackets.
 
		python generate.py [debug]

 6. From the BlockSettle Terminal root directory, go to the `terminal.release` or `terminal.debug` subdirectory, depending on if the `debug` argument was used when generating the environment. On Linux or macOS, execute `make` as one would for any other project. On Windows, open the BlockSettle.sln file so that the binary can be compiled by Visual Studio. Once in VS, select `Build -> Build Solution` from the menu bar, or press `CTRL+SHIFT+B`. No matter which platform you use, you'll hopefully not get any compiler or linker errors.

 7. Assuming the code compiles and links successfully, from the BlockSettle Terminal root directory, go to the `build_terminal` subdirectory. From there, go to the `Release/bin` or `Debug/bin` subdirectories to find the generated binaries. Linux and Windows binaries may be executed directly. On macOS, the user will either have to double-click on the appropriate app or type `open -a Blocksettle\ Terminal` from the command line. If the macOS version needs to have command line arguments passed, the easiest thing to do is to go directly to the actual executable. The binary to execute will be in the `BlockSettle Terminal.app/Contents/MacOS/` subdirectory.

 8. (**WINDOWS ONLY**) A one-time step is required upon the first compilation. Go to `DEV_3RD_ROOT/release/ZeroMQ/lib` and copy the libzmq DLL file to same directory as the BlockSettle Terminal binary. The DLL need only be copied once but it'll have to be re-copied whenever `libzmq` is updated. This will eventually be automated. For now, the build script occasionally crashes when attempting to automate this process.

# Miscellaneous
## Terminal prerequisites
The terminal does need to download and compile some prerequisites in order for the terminal to run. As of Nov. 2018, these prerequisites are:

- [Botan](https://botan.randombit.net/)
- [Crypto++](https://www.cryptopp.com/)  (Will be removed eventually by Botan)
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
- macOS -  In ~/.bash\_profile, export the variable to the desired location. (Example: `export DEV_3RD_ROOT="$HOME/Projects/BlockSettle-alt-location/3rd"`)
