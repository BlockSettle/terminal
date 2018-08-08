import multiprocessing
import os
import shutil
import subprocess

from component_configurator import Configurator


class LibQREncode(Configurator):
    def __init__(self, settings):
        Configurator.__init__(self, settings)
        self._version = 'a50e2db8b0d223383eccf752061c8ae55497961c'
        self._package_name = 'libqrencode'

        self._package_url = 'https://github.com/fukuchi/libqrencode/archive/' + self._version + '.zip'

    def get_package_name(self):
        return self._package_name

    def get_url(self):
        return self._package_url

    def is_archive(self):
        return True

    def get_unpacked_sources_dir(self):
        return os.path.join(self._project_settings.get_sources_dir(), 'libqrencode-' + self._version)

    def config(self):
        command = ['cmake',
                   '-DWITH_TOOLS=NO',
                   self.get_unpacked_sources_dir(),
                   '-G',
                   self._project_settings.get_cmake_generator()]

        result = subprocess.call(command)

        return result == 0

    def make_windows(self):
        command = ['devenv',
                   self.get_solution_file(),
                   '/build',
                   self.get_win_build_configuration(),
                   '/project',
                   'qrencode',
                   '/out',
                   'build.log']

        print(' '.join(command))

        result = subprocess.call(command)
        return result == 0

    def get_solution_file(self):
        return 'QREncode.sln'

    def get_win_build_configuration(self):
        if self._project_settings.get_build_mode() == 'release':
            return 'Release'
        else:
            return 'Debug'

    def install_win(self):
        lib_dir = os.path.join(self.get_build_dir(), self.get_win_build_configuration())
        include_dir = self.get_unpacked_sources_dir()

        install_lib_dir = os.path.join(self.get_install_dir(), 'lib')
        install_include_dir = os.path.join(self.get_install_dir(), 'include')

        self.filter_copy(lib_dir, install_lib_dir, '.lib')
        self.filter_copy(include_dir, install_include_dir, '.h')

        return True

    def make_x(self):
        command = ['make', '-j', str(multiprocessing.cpu_count())]

        result = subprocess.call(command)
        return result == 0

    def install_x(self):
        lib_dir = self.get_build_dir()
        include_dir = self.get_unpacked_sources_dir()

        install_lib_dir = os.path.join(self.get_install_dir(), 'lib')
        install_include_dir = os.path.join(self.get_install_dir(), 'include')

        self.filter_copy(lib_dir, install_lib_dir, '.a')
        self.filter_copy(include_dir, install_include_dir, '.h')

        return True
