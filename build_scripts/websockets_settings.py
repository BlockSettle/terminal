import multiprocessing
import os
import shutil
import subprocess

from component_configurator import Configurator

class WebsocketsSettings(Configurator):
    def __init__(self, settings):
        Configurator.__init__(self, settings)
        self._version = '3.1.0'
        self._package_name = 'libwebsockets'
        self._package_url = 'https://github.com/warmcat/libwebsockets/archive/v' + self._version + '.zip'
        self._script_revision = '5'

    def get_package_name(self):
        return self._package_name + '-' + self._version

    def get_revision_string(self):
        return self._version + '-' + self._script_revision

    def get_install_dir(self):
        return os.path.join(self._project_settings.get_common_build_dir(), 'libwebsockets')

    def get_url(self):
        return self._package_url

    def is_archive(self):
        return True

    def config(self):
        # Removing HTTPS is safe for our purposes. WS connections are secured by
        # BIP 150/151, which basically acts as a lightweight TLS replacement.
        command = ['cmake',
                   os.path.join(self._project_settings.get_sources_dir(), self._package_name + '-' + self._version),
                   '-DLWS_WITHOUT_SERVER=OFF',
                   '-DLWS_SSL_CLIENT_USE_OS_CA_CERTS=0',
                   '-DLWS_WITH_SSL=0',
                   '-DLWS_WITHOUT_TESTAPPS=ON',
                   '-DLWS_WITHOUT_TEST_SERVER=ON',
                   '-DLWS_WITHOUT_TEST_PING=ON',
                   '-DLWS_WITHOUT_TEST_CLIENT=ON']

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

        if self._project_settings.on_linux():
            command.append('-DLWS_WITH_STATIC=ON')
            command.append('-DLWS_WITH_SHARED=ON')

        if self._project_settings.on_windows():
            if self._project_settings.get_link_mode() == 'shared':
                command.append('-DLWS_WITH_STATIC=OFF')
                command.append('-DLWS_WITH_SHARED=ON')
            else:
                command.append('-DLWS_WITH_SHARED=OFF')
                command.append('-DLWS_WITH_STATIC=ON')

        command.append('-G')
        command.append(self._project_settings.get_cmake_generator())

        command.append('-DCMAKE_INSTALL_PREFIX=' + self.get_install_dir())

        print(command)
        env_vars = os.environ.copy()
        result = subprocess.call(command, env=env_vars)
        return result == 0

    def make_windows(self):
        project_name = 'websockets'
        if self._project_settings.get_link_mode() == 'shared':
            project_name = 'websockets_shared'


        command = ['msbuild',
                   self.get_solution_file(),
                   '/t:' + project_name,
                   '/p:Configuration=' + self.get_win_build_configuration(),
                   '/p:CL_MPCount=' + str(max(1, multiprocessing.cpu_count() - 1))]

        print('Start building libwebsockets')
        print(' '.join(command))

        result = subprocess.call(command)
        return result == 0

    def get_solution_file(self):
        return 'libwebsockets.sln'

    def get_win_build_configuration(self):
        if self._project_settings.get_build_mode() == 'release':
            return 'RelWithDebInfo'
        else:
            return 'Debug'

    def install_win(self):
        install_lib_dir = os.path.join(self.get_install_dir(), 'lib')
        install_include_dir = os.path.join(self.get_install_dir(), 'include')

        lib_dir = os.path.join(self.get_build_dir(), 'lib', self.get_win_build_configuration())
        # get includes from unpacked dir because cmake have bug during copying includes to build dir
        include_dir = os.path.join(self.get_unpacked_sources_dir(), 'include')
        self.filter_copy(include_dir, install_include_dir)
        # set once more build dir to copy generated includes
        include_dir = os.path.join(self.get_build_dir(), 'include')

        # copy libs
        if self._project_settings.get_link_mode() == 'shared':
            output_dir = os.path.join(self.get_build_dir(), 'lib', self.get_win_build_configuration())
            self.filter_copy(output_dir, os.path.join(self.get_install_dir(), 'lib'), '.lib')
            output_dir = os.path.join(self.get_build_dir(), 'bin', self.get_win_build_configuration())
            self.filter_copy(output_dir, os.path.join(self.get_install_dir(), 'lib'), '.dll', False)
        else:
            self.filter_copy(lib_dir, install_lib_dir, '.lib')

        self.filter_copy(include_dir, install_include_dir, None, False)

        return True

    def make_x(self):
        command = ['make', '-j', str(multiprocessing.cpu_count())]
        result = subprocess.call(command)
        return result == 0

    def install_x(self):
        command = ['make', 'install']
        result = subprocess.call(command)
        return result == 0
