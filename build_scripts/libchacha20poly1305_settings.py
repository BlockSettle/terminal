import multiprocessing
import os
import shutil
import subprocess

from component_configurator import Configurator

# NB: This is a placeholder of sorts. It allows compilation on MacOS and *NIX
# but there are no inherent files for building Windows. It may be necessary to
# establish a fork, and maybe submit CMake files upstream.
class LibChaCha20Poly1305Settings(Configurator):
    def __init__(self, settings):
        Configurator.__init__(self, settings)
        self._version = '1132bf31c48e214adb296e3de1f96497bc15a1c0'
        self._package_name = 'chacha20poly1305'

        self._package_url = 'https://github.com/droark/chacha20poly1305/archive/1132bf31c48e214adb296e3de1f96497bc15a1c0.zip'

    def get_package_name(self):
        return self._package_name + '-' + self._version

    def get_revision_string(self):
        return self._version

    def get_url(self):
        return self._package_url

    def get_install_dir(self):
        return os.path.join(self._project_settings.get_common_build_dir(), 'libchacha20poly1305')

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

        result = subprocess.call(command)

        return result == 0

    def make_windows(self):
        command = ['devenv',
                   self.get_solution_file(),
                   '/build',
                   self.get_win_build_configuration(),
                   '/project',
                   self._package_name]

        print(' '.join(command))

        result = subprocess.call(command)
        return result == 0

    def get_solution_file(self):
        return 'libchacha20poly1305.sln'

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
