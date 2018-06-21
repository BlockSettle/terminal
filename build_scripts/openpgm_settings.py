import multiprocessing
import os
import shutil
import subprocess

from component_configurator import Configurator

class OpenPGMSettings(Configurator):
    def __init__(self, settings):
        Configurator.__init__(self, settings)
        self._version = '5.2.122'
        self._package_name = 'OpenPGM'

        self._package_url = 'https://storage.googleapis.com/google-code-archive-downloads/v2/code.google.com/openpgm/libpgm-5.2.122.tar.gz'

    def get_package_name(self):
        return self._package_name

    def get_url(self):
        return self._package_url

    def is_archive(self):
        return True

    def config_x(self):
        command = []

        command.append(os.path.join(self.get_unpacked_sources_dir(), 'openpgm', 'pgm', 'configure'))

        command.append('--prefix')
        command.append(self.get_install_dir())

        flags = self._project_settings.get_flags()
        if len(flags) != 0:
            command.append( 'CXXFLAGS='+ flags)
            command.append( 'CFLAGS='  + flags)

        print('Start build openPGM: ' + ' '.join(command))

        result = subprocess.call(command)
        return result == 0

    def config_windows(self):
        command = []

        command.append('cmake')
        command.append(self.get_pgm_sources_root())
        command.append('-G')
        command.append(self._project_settings.get_cmake_generator())
        result = subprocess.call(command)
        return result == 0

    def make_x(self):
        command = []

        command.append('make')
        command.append('-j')
        command.append( str(multiprocessing.cpu_count()) )

        result = subprocess.call(command)
        return result == 0

    def make_windows(self):
        command = []

        command.append('devenv')
        command.append(self.get_solution_file())
        command.append('/build')
        command.append(self.get_win_build_configuration())
        command.append('/project')
        command.append('libpgm')

        result = subprocess.call(command)
        return result == 0

    def get_solution_file(self):
        return 'OpenPGM.sln'

    def get_win_build_configuration(self):
        if self._project_settings.get_build_mode() == 'release':
            return 'Release'
        else:
            return 'Debug'

    def install_x(self):
        command = []

        command.append('make')
        command.append('install')

        result = subprocess.call(command)
        return result == 0

    def install_win(self):
        output_include_dir = os.path.join(self.get_pgm_sources_root(), 'include', 'pgm')
        output_lib_dir = os.path.join(self.get_build_dir(), 'lib', self.get_win_build_configuration())

        install_include_dir = os.path.join(self.get_install_dir(), 'include', 'pgm-5.2', 'pgm')
        install_lib_dir = os.path.join(self.get_install_dir(), 'lib')

        # install libs
        os.makedirs(install_lib_dir)
        for lib_name in os.listdir(output_lib_dir):
            print('Lib:' + lib_name)
        libs = [ lib_name for lib_name in os.listdir(output_lib_dir) if lib_name.endswith('.lib')]

        shutil.copy( os.path.join(output_lib_dir, libs[0]), os.path.join(install_lib_dir, libs[0]))
        shutil.copy( os.path.join(output_lib_dir, libs[0]), os.path.join(install_lib_dir, 'libpgm.lib'))

        # install libs
        shutil.copytree(output_include_dir, install_include_dir)

        return True

    def cflags(self):
        incl1 = os.path.join(self.get_install_dir(), 'include', 'pgm-5.2')
        incl2 = os.path.join(self.get_install_dir(), 'lib', 'pgm-5.2', 'include' )
        return '-I'+incl1 + ' -I' + incl2

    def lflags(self):
        lib_dir = os.path.join(self.get_install_dir(), 'lib')

        return '-L' + lib_dir + ' -lpgm'

    def get_pgm_sources_root(self):
        return os.path.join(self.get_unpacked_sources_dir(), 'openpgm', 'pgm')

    def get_install_include_dir(self):
        return os.path.join(self.get_install_dir(), 'include', 'pgm-5.2')

    def get_install_lib_dir(self):
        return os.path.join(self.get_install_dir(), 'lib')

    def get_unpacked_sources_dir(self):
        return os.path.join(self._project_settings.get_sources_dir(), 'libpgm-5.2.122')