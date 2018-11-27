import multiprocessing
import os
import shutil
import subprocess

from component_configurator import Configurator


class JomSettings(Configurator):
    def __init__(self, settings):
        Configurator.__init__(self, settings)
        self._version = 'Latest'
        self._package_name = 'Jom'

        self._package_url = 'https://download.qt.io/official_releases/jom/jom.zip'

    def get_package_name(self):
        return self._package_name

    def get_revision_string(self):
        return self._version

    def get_url(self):
        return self._package_url

    def is_archive(self):
        return True

    def config(self):
        return True

    def make(self):
        return True

    def install(self):
        self.filter_copy(self.get_unpacked_sources_dir(), os.path.join(self.get_install_dir(), 'bin'), '.exe')
        return True

    def unpack_in_common_dir(self):
        return False

    def get_executable_path(self):
        return os.path.join(self.get_install_dir(), 'bin', 'jom.exe')
