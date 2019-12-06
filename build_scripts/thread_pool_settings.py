import multiprocessing
import os
import shutil
import subprocess
import sys

from component_configurator import Configurator


class ThreadPoolSettings(Configurator):
    def __init__(self, settings):
        Configurator.__init__(self, settings)
        self._package_name = 'thread-pool-cpp'
        self._script_revision = '1'
        self._git_revision = 'af95dd88daa094f67bbd178b639c7282373a3b09'
        self._package_url = 'https://github.com/inkooboo/thread-pool-cpp/archive/' + self._git_revision + '.zip'

    def get_package_name(self):
        return self._package_name + '-' + self._git_revision

    def get_revision_string(self):
        return self._git_revision + '_' + self._script_revision

    def get_url(self):
        return self._package_url

    def get_install_dir(self):
        return os.path.join(self._project_settings.get_common_build_dir(), 'thread-pool-cpp')

    def is_archive(self):
        return True

    def config(self):
        return True

    def make(self):
        return True

    def install(self):
        self.filter_copy(self.get_unpacked_sources_dir(), self.get_install_dir())
        return True
