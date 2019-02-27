import multiprocessing
import os
import subprocess

from component_configurator import Configurator


class gRPCSettings(Configurator):
    def __init__(self, settings):
        Configurator.__init__(self, settings)
        self._version = '1.18.0'
        self._package_name = 'grpc-' + self._version
        self._package_url = 'https://github.com/grpc/grpc/archive/v' + self._version + '.tar.gz'

    def get_package_name(self):
        return self._package_name

    def get_revision_string(self):
        return self._version

    def get_url(self):
        return self._package_url

    def get_install_dir(self):
        return os.path.join(self._project_settings.get_common_build_dir(), 'gRPC')

    def is_archive(self):
        return True

    def config_windows(self):
        # need to use CMake here
        return False

    def config_x(self):
        self.copy_sources_to_build()
        return True

    def make_windows(self):
        return False

    def make_x(self):
        command = ['make', '-j', str(multiprocessing.cpu_count())]
        result = subprocess.call(command)
        return result == 0

    def install_win(self):
        return False

    def install_x(self):
        command = ['make', 'prefix='+self.get_install_dir(), 'install']
        result = subprocess.call(command)
        return result == 0
