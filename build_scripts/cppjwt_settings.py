import os
import subprocess
import shutil

from component_configurator import Configurator


class CppJwtSettings(Configurator):
    def __init__(self, settings):
        Configurator.__init__(self, settings)
        self._version = '1.1.1'
        self._package_name = 'cpp-jwt-' + self._version
        self._package_url = "https://github.com/arun11299/cpp-jwt/archive/v" + self._version + ".zip"

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
        return os.path.join(self._project_settings.get_common_build_dir(), 'cpp-jwt')

    def install(self):
        self.filter_copy(self.get_unpacked_sources_dir(), self.get_install_dir())
        return True
