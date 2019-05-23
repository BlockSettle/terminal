import multiprocessing
import os
import shutil
import subprocess
import sys

from component_configurator import Configurator


class BotanSettings(Configurator):
    def __init__(self, settings, enableSqlite = False, enablePKCS11 = False):
        Configurator.__init__(self, settings)
        self._version = '2.10.0'
        self._package_name = 'botan'
        self._script_revision = '2'
        self._enableSqlite = enableSqlite
        self._enablePKCS11 = enablePKCS11

        self._package_url = 'https://github.com/randombit/botan/archive/' + self._version + '.zip'

    def get_package_name(self):
        return self._package_name + '-' + self._version

    def get_revision_string(self):
        return self._version + self._script_revision

    def get_url(self):
        return self._package_url

    def get_install_dir(self):
        return os.path.join(self._project_settings.get_common_build_dir(), 'botan')

    def is_archive(self):
        return True

    def config(self):
        command = [sys.executable,
                   self.get_unpacked_sources_dir() + '/configure.py',
                   '--without-documentation',
        ]

        if self._enableSqlite:
            command += ['--with-sqlite3']

        if not self._enablePKCS11:
            command += ['--disable-modules=pkcs11']

        if self._project_settings.get_link_mode() == 'static':
            command.append('--disable-shared-library')
        else:
            command.append('--enable-shared-library')

        command.append('--prefix=' + self.get_install_dir())

        if self._project_settings.on_windows():
            self._build_tool = [os.path.join(self._project_settings.get_common_build_dir(), 'Jom/bin/jom.exe')]
            if self._project_settings.get_link_mode() == 'static':
                if self._project_settings.get_build_mode() == 'release':
                    command.append('--msvc-runtime=MT')
                else:
                    command.append('--msvc-runtime=MTd')
            else:
                if self._project_settings.get_build_mode() == 'release':
                    command.append('--msvc-runtime=MD')
                else:
                    command.append('--msvc-runtime=MDd')
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
