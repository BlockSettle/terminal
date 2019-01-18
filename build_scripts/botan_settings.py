import multiprocessing
import os
import shutil
import subprocess

from component_configurator import Configurator


class BotanSettings(Configurator):
    def __init__(self, settings):
        Configurator.__init__(self, settings)
        self._version = '2.9.0'
        self._package_name = 'botan'

        self._package_url = 'https://github.com/randombit/botan/archive/' + self._version + '.zip'

    def get_package_name(self):
        return self._package_name + '-' + self._version

    def get_revision_string(self):
        return self._version

    def get_url(self):
        return self._package_url

    def get_install_dir(self):
        return os.path.join(self._project_settings.get_common_build_dir(), 'botan')

    def is_archive(self):
        return True

    def config(self):
        command = ['python',
                   self.get_unpacked_sources_dir() + '/configure.py',
                   '--disable-modules=pkcs11',
                   '--without-documentation',
                   '--disable-shared-library',
                   '--prefix=' + self.get_install_dir(),
        ]

        if self._project_settings.on_windows():
            self._build_tool = [os.path.join(self._project_settings.get_common_build_dir(), 'Jom/bin/jom.exe')]
            if self._project_settings.get_build_mode() == 'release':
                command.append('--msvc-runtime=MT')
            else:
                command.append('--msvc-runtime=MTd')
        else:
            self._build_tool = ['make', '-j', str(multiprocessing.cpu_count())]
            if self._project_settings.get_build_mode() == 'release':
                pass
            else:
                command.append('--debug-mode')

        result = subprocess.call(command)

        return result == 0

    def make(self):
        command = self._build_tool
        result = subprocess.call(command)
        return result == 0

    def install(self):
        command = self._build_tool + ['install']
        result = subprocess.call(command)
        return True
