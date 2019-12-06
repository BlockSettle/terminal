import os
import subprocess
import shutil
import multiprocessing

from component_configurator import Configurator

class QtMustache(Configurator):
    def __init__(self, settings):
        Configurator.__init__(self, settings)
        self._version = 'a61f7385790ee33a051082957791f4859555e154'
        self._package_name = 'qt-mustache-' + self._version
        self._package_url = 'https://github.com/robertknight/qt-mustache/archive/' + self._version + '.zip'

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
        return os.path.join(self._project_settings.get_common_build_dir(), "qt-mustache")

    def install(self):
        self.filter_copy(self.get_unpacked_sources_dir() + "/src", self.get_install_dir())
        return True
