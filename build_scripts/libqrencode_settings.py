import multiprocessing
import os
import shutil
import subprocess

from component_configurator import Configurator


class LibQREncode(Configurator):
    def __init__(self, settings):
        Configurator.__init__(self, settings)
        self._version = '13b159f9d9509b0c9f5ca0df7a144638337ddb15'
        self._package_name = 'libqrencode'
        self._script_revision = '3'

        self._package_url = 'https://github.com/fukuchi/libqrencode/archive/' + self._version + '.zip'

    def get_package_name(self):
        return self._package_name + '-' + self._version

    def get_revision_string(self):
        return self._version + '-' + self._script_revision

    def get_url(self):
        return self._package_url

    def is_archive(self):
        return True

    def get_install_dir(self):
        return os.path.join(self._project_settings.get_common_build_dir(), 'LibQREncode')

    def config(self):
        command = ['cmake',
                   '-DWITH_TOOLS=NO',
                   self.get_unpacked_sources_dir(),
                   '-G',
                   self._project_settings.get_cmake_generator()]

        # for static lib
        if self._project_settings.on_windows() and self._project_settings.get_link_mode() != 'shared':
            if self._project_settings.get_build_mode() == 'debug':
                command.append('-DCMAKE_C_FLAGS_DEBUG=/D_DEBUG /MTd /Zi /Ob0 /Od /RTC1')
                command.append('-DCMAKE_CXX_FLAGS_DEBUG=/D_DEBUG /MTd /Zi /Ob0 /Od /RTC1')
            else:
                command.append('-DCMAKE_C_FLAGS_RELEASE=/MT /O2 /Ob2 /D NDEBUG')
                command.append('-DCMAKE_CXX_FLAGS_RELEASE=/MT /O2 /Ob2 /D NDEBUG')
                command.append('-DCMAKE_C_FLAGS_RELWITHDEBINFO=/MT /O2 /Ob2 /D NDEBUG')
                command.append('-DCMAKE_CXX_FLAGS_RELWITHDEBINFO=/MT /O2 /Ob2 /D NDEBUG')

        if self._project_settings.get_link_mode() == "shared":
            command.append('-DBUILD_SHARED_LIBS=YES')

        command.append('-DCMAKE_INSTALL_PREFIX=' + self.get_install_dir())

        result = subprocess.call(command)

        return result == 0

    def make_windows(self):
        command = ['msbuild',
                   self.get_solution_file(),
                   '/t:qrencode',
                   '/p:Configuration=' + self.get_win_build_configuration(),
                   '/p:CL_MPCount=' + str(max(1, multiprocessing.cpu_count() - 1))]

        print(' '.join(command))

        result = subprocess.call(command)
        return result == 0

    def get_solution_file(self):
        return 'QREncode.sln'

    def get_win_build_configuration(self):
        if self._project_settings.get_build_mode() == 'release':
            return 'RelWithDebInfo'
        else:
            return 'Debug'

    def install_win(self):
        lib_dir = os.path.join(self.get_build_dir(), self.get_win_build_configuration())
        include_dir = self.get_unpacked_sources_dir()

        install_lib_dir = os.path.join(self.get_install_dir(), 'lib')
        install_include_dir = os.path.join(self.get_install_dir(), 'include')

        # copy libs
        if self._project_settings.get_link_mode() == 'shared':
            output_dir = os.path.join(self.get_build_dir(),  self.get_win_build_configuration())
            self.filter_copy(output_dir, os.path.join(self.get_install_dir(), 'lib'), '.dll')
            self.filter_copy(output_dir, os.path.join(self.get_install_dir(), 'lib'), '.lib', False)
        else:
            self.filter_copy(lib_dir, install_lib_dir, '.lib')

        self.filter_copy(include_dir, install_include_dir, '.h')

        return True

    def make_x(self):
        command = ['make', '-j', str(multiprocessing.cpu_count())]
        result = subprocess.call(command)
        return result == 0

    def install_x(self):
        command = ['make', 'install']
        result = subprocess.call(command)
        return result == 0
