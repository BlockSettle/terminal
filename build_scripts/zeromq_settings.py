import os
import subprocess
import shutil
import multiprocessing

from component_configurator import Configurator

class ZeroMQSettings(Configurator):
    def __init__(self, settings):
        Configurator.__init__(self, settings)
        self._version = '4.3.2'
        self._script_revision = '2'

        if settings.on_windows():
            self._package_name = 'libzmq-' + self._version
            self._package_url = 'https://github.com/zeromq/libzmq/archive/v' + self._version + '.zip'
        else:
            # download linux/osx release source package. simply to avoid reconfigure
            self._package_name = 'zeromq-' + self._version
            self._package_url = 'https://github.com/zeromq/libzmq/releases/download/v' + self._version + \
                                '/' + self._package_name + '.tar.gz'

    def get_package_name(self):
        return self._package_name

    def get_revision_string(self):
        return self._version + '-' + self._script_revision

    def get_url(self):
        return self._package_url

    def get_install_dir(self):
        return os.path.join(self._project_settings.get_common_build_dir(), 'ZeroMQ')

    def is_archive(self):
        return True

    def config_windows(self):
        command = []

        # patch cmake file`
        cmakeFileName = os.path.abspath(os.path.join(self.get_unpacked_sources_dir(), 'CMakeLists.txt'))
        with open(cmakeFileName, 'r') as f:
            lines = [line for line in f]

        for index, line in enumerate(lines):
            if 'set (ZMQ_USE_TWEETNACL 1)' in line:
                lines[index] = line.replace('set (ZMQ_USE_TWEETNACL 1)', 'ADD_DEFINITIONS(-DZMQ_USE_TWEETNACL)')
            elif 'set (ZMQ_HAVE_CURVE 1)' in line:
                lines[index] = line.replace('set (ZMQ_HAVE_CURVE 1)', 'ADD_DEFINITIONS(-DZMQ_HAVE_CURVE)')
            elif 'set (ZMQ_USE_LIBSODIUM 1)' in line:
                lines[index] = line.replace('set (ZMQ_USE_LIBSODIUM 1)', 'ADD_DEFINITIONS(-DZMQ_USE_LIBSODIUM)')

        with open(cmakeFileName, 'w') as f:
            for line in lines:
                f.write(line)

        command.append('cmake')
        command.append(self.get_unpacked_sources_dir())
        command.append('-DZMQ_BUILD_TESTS=OFF')

        if self._project_settings.get_link_mode() == 'shared':
            command.append('-DBUILD_STATIC=OFF')

        # for static lib
        if self._project_settings.on_windows() and self._project_settings.get_link_mode() != 'shared':
            if self._project_settings.get_build_mode() == 'debug':
                command.append('-DCMAKE_C_FLAGS_DEBUG="/D_DEBUG /MTd /Zi /Ob0 /Od /RTC1"')
                command.append('-DCMAKE_CXX_FLAGS_DEBUG="/D_DEBUG /MTd /Zi /Ob0 /Od /RTC1"')
            else:
                command.append('-DCMAKE_C_FLAGS_RELEASE="/MT /O2 /Ob2 /D NDEBUG"')
                command.append('-DCMAKE_CXX_FLAGS_RELEASE="/MT /O2 /Ob2 /D NDEBUG"')
                command.append('-DCMAKE_C_FLAGS_RELWITHDEBINFO="/MT /O2 /Ob2 /D NDEBUG"')
                command.append('-DCMAKE_CXX_FLAGS_RELWITHDEBINFO="/MT /O2 /Ob2 /D NDEBUG"')
                
        command.append('-G')
        command.append(self._project_settings.get_cmake_generator())

        result = subprocess.call(command)
        return result == 0

    def config_x(self):
        self.copy_sources_to_build()

        reconf_command = ['./autogen.sh']
        result = subprocess.call(reconf_command)

        if result != 0:
            return False

        command = ['./configure',
                   '--enable-libunwind=no',
                   '--verbose',
                   '--prefix',
                   self.get_install_dir(),
                   '--without-libsodium' ]

        result = subprocess.call(command)
        return result == 0

    def get_vs_project_root(self):
        return os.path.join(self.get_build_dir(), 'builds', 'msvc', self.get_vs_version())

    def get_solution_file(self):
        return os.path.join('ZeroMQ.sln')

    def make_windows(self):
        command = ['msbuild',
                   self.get_solution_file(),
                   '/t:libzmq',
                   '/p:Configuration=' + self.get_win_configuration(),
                   '/p:CL_MPCount=' + str(max(1, multiprocessing.cpu_count() - 1))]

        result = subprocess.call(command)

        return result == 0

    def get_win_configuration(self):
        if self._project_settings.get_build_mode() == 'release':
            return 'RelWithDebInfo'
        else:
            return 'Debug'

    def get_win_configuration_output_dir(self):
        return self.get_win_configuration()

    def get_win_platform(self):
        return 'x64'

    def get_vs_version(self):
        vs_year = self.get_vs_year()
        if vs_year > 2015:
            return 'vs2015'
        return 'vs' + vs_year

    def make_x(self):
        command = ['make', '-j', str(multiprocessing.cpu_count())]

        result = subprocess.call(command)
        return result == 0

    def install_win(self):
        print('Installing ZeroMQ')

        output_dir = self.get_win_configuration_output_dir()

        src_lib_dir = os.path.join(self.get_build_dir(), 'lib', output_dir)
        src_dll_dir = os.path.join(self.get_build_dir(), 'bin', output_dir)

        install_lib_dir = os.path.join(self.get_install_dir(), 'lib')

        self.filter_copy(src_lib_dir, install_lib_dir)
        self.filter_copy(src_dll_dir, install_lib_dir, cleanupDst=False)

        include_dir = self.get_include_dir_win()
        install_include_dir = os.path.join(self.get_install_dir(), 'include')
        self.filter_copy(include_dir, install_include_dir)

        return True

    # return output directory for libzmq project
    def get_bin_dir_win(self):
        if self._project_settings.get_vs_version_number() == '14':
            toolset = 'v140'
        else:
            toolset = 'v120'

        return os.path.join(self.get_build_dir(), 'bin', self.get_win_platform(),
                            self.get_win_configuration_output_dir(), toolset, 'dynamic')

    def get_include_dir_win(self):
        return os.path.join(self.get_unpacked_sources_dir(), 'include')

    def install_x(self):
        command = ['make', 'install']

        result = subprocess.call(command)
        return result == 0
