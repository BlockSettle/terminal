import os
import subprocess
import shutil
import multiprocessing

from component_configurator import Configurator

class Json11(Configurator):
    def __init__(self, settings):
        Configurator.__init__(self, settings)
        self._version = '1.0.0'
        self._package_name = 'json11-' + self._version
        self._package_url = 'https://github.com/dropbox/json11/archive/v' + self._version + '.zip'

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

    def get_install_dir(self):
        return os.path.join(self._project_settings.get_common_build_dir(), "json11")

    def install(self):
        self.filter_copy(self.get_unpacked_sources_dir(), self.get_install_dir())
        return True
