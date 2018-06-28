import multiprocessing
import os
import shutil
import subprocess

from component_configurator import Configurator


class OpenSslSettings(Configurator):
    def __init__(self, settings):
        Configurator.__init__(self, settings)
        self._major_ver = '1'
        self._minor_ver = '1'
        self._patch_ver = '0h'
        self._version = self._major_ver + '_' + self._minor_ver + '_' + self._patch_ver
        self._package_name = 'openssl-OpenSSL_' + self._version

        self._package_url = 'https://github.com/openssl/openssl/archive/OpenSSL_' + self._version + '.tar.gz'

    def get_package_name(self):
        return self._package_name

    def get_url(self):
        return self._package_url

    def get_install_dir(self):
        return os.path.join(self._project_settings.get_common_build_dir(), 'OpenSSL')

    def is_archive(self):
        return True

    def config_windows(self):
        self.copy_sources_to_build()

        print('Generating openssl project')
        print('Please make sure you have installed Perl and NASM (and both of them are in %PATH%)')

        command = ['perl', 'Configure', 'no-shared', '--prefix='+self.get_install_dir(),
                   '--openssldir='+self.get_install_dir(), 'VC-WIN64A']

        result = subprocess.call(command)
        return result == 0

    def config_x(self):
        self.copy_sources_to_build()

        command = ['./config', 'no-shared',
                   '--prefix='+self.get_install_dir(), '--openssldir='+self.get_install_dir()]

        result = subprocess.call(command)
        return result == 0

    def make(self):
        command = []

        if self._project_settings.on_windows():
            command.append('nmake')
        else:
            command.append('make')
            command.append('-j')
            command.append(str(max(1, multiprocessing.cpu_count() - 1)))

        result = subprocess.call(command)
        if result != 0:
            print('OpenSSL make failed')
            return False

        return True


    def install(self):
        command = []
        if self._project_settings.on_windows():
            command.append('nmake')
        else:
            command.append('make')

        command.append('install')
        result = subprocess.call(command)
        if result != 0:
            print('OpenSSL install failed')
            return False

        return True
