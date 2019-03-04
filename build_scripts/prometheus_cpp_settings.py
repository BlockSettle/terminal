import multiprocessing
import os
import shutil
import subprocess

from component_configurator import Configurator


class PrometheusCpp(Configurator):
    def __init__(self, settings):
        Configurator.__init__(self, settings)
        self._version = '83e329c5512aa8dc4850a03d70621188d6fb92be'
        self._package_name = 'PrometheusCpp'
        self._package_dir_name = 'PrometheusCpp'

        self._git_url = 'https://github.com/Ation/prometheus-cpp.git'

    def get_package_name(self):
        return self._package_name + '-' + self._version

    def get_revision_string(self):
        return self._version

    def is_archive(self):
        return False

    def get_install_dir(self):
        return os.path.join(self._project_settings.get_common_build_dir(), self._package_name)

    def getSourcesDir(self):
        return os.path.join(self._project_settings.get_sources_dir(), self._package_name)

    def download_package(self):
        # git clone
        self.remove_fs_object(self.getSourcesDir())

        command = [
            'git',
            'clone',
            self._git_url,
            self.getSourcesDir()
        ]

        print(' '.join(command))
        result = subprocess.call(command)
        if result != 0:
            return False

        cwd = os.getcwd()
        os.chdir(self.getSourcesDir())

        command = [
            'git',
            'checkout',
            self._version
            ]
        result = subprocess.call(command)
        if result != 0:
            return False

        command = [
            'git',
            'submodule',
            'init',
            '3rdparty/civetweb'
            ]
        result = subprocess.call(command)
        if result != 0:
            return False

        command = [
            'git',
            'submodule',
            'update'
            ]
        result = subprocess.call(command)
        if result != 0:
            return False

        os.chdir(cwd)
        return True

    def config(self):
        command = ['cmake',
                   self.get_unpacked_sources_dir(),
                   '-G',
                   self._project_settings.get_cmake_generator()]

        command.append('-DENABLE_TESTING=OFF')
        command.append('-DENABLE_PUSH=OFF')
        command.append('-DCMAKE_INSTALL_PREFIX='+self.get_install_dir())

        result = subprocess.call(command)

        return result == 0

    def make_windows(self):
        return True

    def install_win(self):
        return True

    def make_x(self):
        command = ['make', '-j', str(multiprocessing.cpu_count())]

        result = subprocess.call(command)
        return result == 0

    def install_x(self):
        command = ['make', 'install']

        result = subprocess.call(command)
        return result == 0
