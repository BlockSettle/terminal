import multiprocessing
import os
import shutil
import subprocess

from component_configurator import Configurator


class CurlSettings(Configurator):
    def __init__(self, settings):
        Configurator.__init__(self, settings)
        self._version = '7_63_0'
        self._script_revision = '3'
        self._package_name = 'curl-' + self._version
        self._package_url = 'https://github.com/curl/curl/archive/' + self._package_name + '.tar.gz'
	self._package_dir_name = 'curl-' + self._package_name

    def get_package_name(self):
        return self._package_name

    def get_revision_string(self):
        return self._version + '-' + self._script_revision

    def get_url(self):
        return self._package_url

    def get_install_dir(self):
        return os.path.join(self._project_settings.get_common_build_dir(), 'curl')

    def is_archive(self):
        return True

    def config(self):
        self.copy_sources_to_build()

        print('Generating curl solution')

        command = ['cmake',
            '-G',
            self._project_settings.get_cmake_generator(),
            '-DCURL_DISABLE_FTP=ON',
            '-DCURL_DISABLE_LDAP=ON',
            '-DCURL_DISABLE_LDAPS=ON',
            '-DCURL_DISABLE_TELNET=ON',
            '-DCURL_DISABLE_DICT=ON',
            '-DCURL_DISABLE_FILE=ON',
            '-DCURL_DISABLE_TFTP=ON',
            '-DCURL_DISABLE_RTSP=ON',
            '-DCURL_DISABLE_POP3=ON',
            '-DCURL_DISABLE_IMAP=ON',
            '-DCURL_DISABLE_GOPHER=ON',
            '-DCMAKE_USE_OPENSSL=ON',
            '-DOPENSSL_ROOT_DIR=' + os.path.join(self._project_settings.get_common_build_dir(), 'OpenSSL'),
            '-DCMAKE_INSTALL_PREFIX=' + self.get_install_dir(),
            '-DBUILD_SHARED_LIBS=OFF',
            '-DBUILD_CURL_EXE=OFF',
            '-DBUILD_TESTING=OFF',
        ]

        result = subprocess.call(command)
        return result == 0

    def get_solution_file(self):
        return os.path.join(self.get_build_dir(), 'CURL.sln')

    def make_windows(self):
        print('Making curl: might take a while')
        command = ['devenv',
                   self.get_solution_file(),
                   '/build',
                   self.get_win_build_mode(),
                   '/project', 'curl']

        result = subprocess.call(command)
        return result == 0

    def get_win_build_mode(self):
        if self._project_settings.get_build_mode() == 'release':
            return 'RelWithDebInfo'
        else:
            return 'Debug'

    def make_x(self):
        command = ['make', '-j', str(multiprocessing.cpu_count())]

        result = subprocess.call(command)
        return result == 0

    def install_win(self):
        # copy libs
        output_dir = os.path.join(self.get_install_dir(), 'lib')
        lib_dir = os.path.join(self.get_build_dir(), 'lib', self.get_win_build_mode())
        print('copy from ' + lib_dir + ' to ' + output_dir)
        self.filter_copy(lib_dir, output_dir, '.lib')

        # copy headers
        output_dir = os.path.join(self.get_install_dir(), 'include')
        inc_dir = os.path.join(self.get_build_dir(), 'include')
        self.filter_copy(inc_dir, output_dir, '.h')

        return True

    def install_x(self):
        command = ['make', 'install']
        result = subprocess.call(command)
        if result != 0:
            print('Failed to install Curl')
            return False

        return True
