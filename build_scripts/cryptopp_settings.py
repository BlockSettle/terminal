import multiprocessing
import os
import shutil
import subprocess

from component_configurator import Configurator


class CryptoppSettings(Configurator):
    def __init__(self, settings):
        Configurator.__init__(self, settings)
        self._version = '32f715f1d723e2bbb78b44fa9e167da64214e2e6'
        self._script_revision = '1'
        self._package_name = 'cryptopp'

        self._package_url = 'https://github.com/weidai11/cryptopp/archive/32f715f1d723e2bbb78b44fa9e167da64214e2e6.zip'

    def get_package_name(self):
        return self._package_name + '-' + self._version

    def get_revision_string(self):
        return self._version + '-' + self._script_revision

    def get_url(self):
        return self._package_url

    def get_install_dir(self):
        return os.path.join(self._project_settings.get_common_build_dir(), 'cryptopp')

    def is_archive(self):
        return True

    def config(self):
        command = ['cmake',
                   self.get_unpacked_sources_dir(),
                   '-G',
                   self._project_settings.get_cmake_generator()]

        if self._project_settings.on_windows():
            command.append('-DCMAKE_CXX_FLAGS_DEBUG=/MTd')
            command.append('-DCMAKE_CXX_FLAGS_RELEASE=/MT')
            command.append('-DCMAKE_CXX_FLAGS_RELWITHDEBINFO=/MT')
        else:
            command.append('-DDISABLE_NATIVE_ARCH=1')

        result = subprocess.call(command)

        return result == 0

    def make_windows(self):
        command = ['devenv',
                   self.get_solution_file(),
                   '/build',
                   self.get_win_build_configuration()]

        result = subprocess.call(command)
        return result == 0

    def get_solution_file(self):
        return 'cryptopp.sln'

    def get_win_build_configuration(self):
        if self._project_settings.get_build_mode() == 'release':
            return 'RelWithDebInfo'
        else:
            return 'Debug'

    def install_win(self):
        lib_dir = os.path.join(self.get_build_dir(), self.get_win_build_configuration())
        include_dir = self.get_unpacked_sources_dir()

        install_lib_dir = os.path.join(self.get_install_dir(), 'lib')
        install_include_dir = os.path.join(self.get_install_dir(), 'include/cryptopp')

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
        install_include_dir = os.path.join(self.get_install_dir(), 'include/cryptopp')

        self.filter_copy(lib_dir, install_lib_dir, '.a')
        self.filter_copy(include_dir, install_include_dir, '.h')

        return True
