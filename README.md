# Supported platforms
Linux x64 (Ubuntu), MacOS X, Windows x64 with VS2015 or VS2017 15.7.3 or up.

# Before building (pre-requisites)
## Windows
* Install Python 2.7 (3.6 wasn't fully tested, yet)
* Install cmake
* Install MySQL Connector from https://dev.mysql.com/downloads/windows/installer/ and choose the web installer (select both Connector/C and Connector/C++ for installation)
* Install YASM from https://yasm.tortall.net/Download.html (download Win64 ZIP archive and unpack it to c:\Program Files\yasm)
* Install ActiveState Perl (https://www.activestate.com/ActivePerl)
* Install NASM (http://www.nasm.us) and add it to global PATH variable

## Ubuntu
* Install python-pip package (it will install Python)
* Install cmake
* Install Qt5 build pre-requisites by running `apt build-dep qt5-default`
* Install libmysqlclient-dev package
* Install autoconf and libtool packages
* Install yasm package
* Install nasm package

## MacOS
 1. Install and update [Homebrew](https://brew.sh). Warnings can probably be ignored, although environment differences and changes Apple makes to the OS between major releases make it impossible to provide definitive guidance.

		/usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
		touch ~/.bashrc
		echo "export CFLAGS=\"-arch x86_64\"" >> ~/.bashrc
		echo "export ARCHFLAGS=\"-arch x86_64\"" >> ~/.bashrc
		source ~/.bashrc
		brew update
		brew doctor

 2. Install and link dependencies required by the BlockSettle terminal build process but not by the final binaries.

		brew install python2 cmake mysql-connector-c mysql-connector-c++ automake libtool yasm nasm xz pkg-config
		echo 'export PATH="/usr/local/opt/mysql-client/bin:$PATH"' >> ~/.bash_profile
		echo 'export PATH="/usr/local/opt/qt/bin:$PATH"' >> ~/.bash_profile
		source ~/.bash_profile

 3. Reset the build machine. (This is necessary due to issues related to the Python install.)

 4. Create a symbolic link for glibtoolize. This requires sudo access and is probably not strictly necessary. It makes Autotools much happier, though, and should be harmless otherwise.

		sudo ln -s /usr/local/bin/glibtoolize /usr/local/bin/libtoolize

 5. Perform the steps seen in the ["Common to all platforms"](#common-to-all-platforms) section. The following command completes the secion's requirements as of Aug. 2018.

		pip install wget requests

 6. Clone the repo, as seen in the ["Cloning the repo"](#cloning-the-repo) section. If asked for your GitHub user/pass, get in touch with the devs and confirm that you have proper access to the repos (`terminal` and the submodules). If you continue to have access issues, confirm whether or not you use 2FA. If you do, you need to use a [Personal Access Token](https://help.github.com/articles/creating-a-personal-access-token-for-the-command-line/) and use the token as your password. The PAT should be separate from any other PATs in case the build machine is compromised; the PAT can be revoked without breaking uncompromised PATs.

		git submodule init
		git submodult update

 7. Build the terminal, as seen in the ["Building BlockSettle terminal"](#building-blocksettle-terminal) section. Note that the script is untested when using Python 3 on Macs. In addition, note how, when Python is installed by `brew`, the actual Python 2 command is `python2`.
 
		python2 generate.py [debug]

## Common to all platforms
Install the following Python modules (pip install):
* wget
* requests

# Cloning the repo
Use your favourite git command to clone this repo, then enter it and execute:
* git submodule init
* git submodule update

# Building BlockSettle terminal

* Run the following command:
`python generate.py [debug]`
(last optional argument is used to enable debugging symbols)

* Go to `terminal.debug` or `terminal.release` dir (depending on the 'debug' argument on the previous step) and type your favourite make command (basically `make -j4`). Windows users should open the BlockSettle.sln file in one of these dirs.

* The binary can then be found in build_terminal dir
