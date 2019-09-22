import multiprocessing
import os
import shutil
import subprocess

from component_configurator import Configurator


class GtestSettings(Configurator):
    def __init__(self, settings):
        Configurator.__init__(self, settings)
        self._version = '1.8.1'
        self._script_revision = '1'
        self._package_name = 'Gtest'

        if settings.on_windows():
            self._package_url = 'https://github.com/google/googletest/archive/release-' + self._version + '.zip'
        else:
            self._package_url = 'https://github.com/google/googletest/archive/release-' + self._version + '.tar.gz'

    def get_package_name(self):
        return self._package_name + '-' + self._version

    def get_revision_string(self):
        return self._version + '-' + self._script_revision

    def get_url(self):
        return self._package_url

    def is_archive(self):
        return True

    def get_install_dir(self):
        return os.path.join(self._project_settings.get_common_build_dir(), 'Gtest')

    def get_unpacked_gtest_sources_dir(self):
        return os.path.join(self._project_settings.get_sources_dir(), 'googletest-release-' + self._version)

    def config(self):
        command = ['cmake',
                   self.get_unpacked_gtest_sources_dir(),
                   '-DBUILD_GTEST=ON',
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
                   'gtest',
                   '/project',
                   'gtest_main']

        print('Start building GTest')
        print(' '.join(command))

        result = subprocess.call(command)
        return result == 0

    def get_solution_file(self):
        return 'googletest-distribution.sln'

    def get_win_build_configuration(self):
        if self._project_settings.get_build_mode() == 'release':
            return 'RelWithDebInfo'
        else:
            return 'Debug'

    def install_win(self):
        lib_dir = os.path.join(self.get_build_dir(), 'googlemock', 'gtest', self.get_win_build_configuration())
        include_dir = os.path.join(self.get_unpacked_gtest_sources_dir(), 'googletest', 'include')

        install_lib_dir = os.path.join(self.get_install_dir(), 'lib')
        install_include_dir = os.path.join(self.get_install_dir(), 'include')

        self.filter_copy(lib_dir, install_lib_dir, '.lib')
        self.filter_copy(include_dir, install_include_dir)

        return True

    def make_x(self):
        command = ['make', '-j', str(multiprocessing.cpu_count())]

        result = subprocess.call(command)
        return result == 0

    def install_x(self):
        include_dir = os.path.join(self.get_unpacked_gtest_sources_dir(), 'googletest/include')
        lib_dir = os.path.join(self.get_build_dir(), 'googlemock/gtest')

        install_lib_dir = os.path.join(self.get_install_dir(), 'lib')
        install_include_dir = os.path.join(self.get_install_dir(), 'include')

        self.filter_copy(lib_dir, install_lib_dir, '.a')
        self.filter_copy(include_dir, install_include_dir)

        return True
