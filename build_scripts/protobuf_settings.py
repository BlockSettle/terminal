import multiprocessing
import os
import shutil
import subprocess

from component_configurator import Configurator


class ProtobufSettings(Configurator):
    def __init__(self, settings):
        Configurator.__init__(self, settings)
        self._version = '3.9.0'
        self._package_name = 'protobuf-' + self._version
        self._package_name_url = 'protobuf-cpp-' + self._version
        self._script_revision = '3'

        self._package_url = 'https://github.com/protocolbuffers/protobuf/releases/download/v' + \
            self._version + '/' + self._package_name_url + '.tar.gz'

    def get_package_name(self):
        return self._package_name

    def get_revision_string(self):
        return self._version + '-' + self._script_revision

    def get_url(self):
        return self._package_url

    def get_install_dir(self):
        return os.path.join(self._project_settings.get_common_build_dir(), 'Protobuf')

    def is_archive(self):
        return True

    def config_windows(self):
        self.copy_sources_to_build()

        print('Generating protobuf solution')

        command = ['cmake',
                   os.path.join(self.get_unpacked_sources_dir(), 'cmake'),
                   '-G',
                   self._project_settings.get_cmake_generator(),
                   '-Dprotobuf_BUILD_TESTS=OFF',
                   '-Dprotobuf_WITH_ZLIB=OFF']

        if self._project_settings.get_link_mode() == 'shared':
            command.append('-Dprotobuf_MSVC_STATIC_RUNTIME=OFF')
        else:
            command.append('-Dprotobuf_MSVC_STATIC_RUNTIME=ON')

        result = subprocess.call(command)
        return result == 0

    def get_solution_file(self):
        return os.path.join(self.get_build_dir(), 'protobuf.sln')

    def config_x(self):
        cwd = os.getcwd()
        os.chdir(self.get_unpacked_sources_dir())
        command = ['./autogen.sh']
        result = subprocess.call(command)
        if result != 0:
            return False

        os.chdir(cwd)
        command = [os.path.join(self.get_unpacked_sources_dir(), 'configure'),
                   '--prefix',
                   self.get_install_dir()]

        result = subprocess.call(command)
        return result == 0

    def make_windows(self):
        print('Making protobuf: might take a while')

        command = ['msbuild',
                   self.get_solution_file(),
                   '/t:protoc',
                   '/p:Configuration=' + self.get_win_build_mode(),
                   '/p:CL_MPCount=' + str(max(1, multiprocessing.cpu_count() - 1))]

        result = subprocess.call(command)
        return result == 0

    def get_win_build_mode(self):
        if self._project_settings.get_build_mode() == 'release':
            return 'RelWithDebInfo'
        else:
            return 'Debug'

    def make_x(self):
        command = ['make', '-j', str(multiprocessing.cpu_count())]

        result = subprocess.call(command)
        return result == 0

    def install_win(self):
        output_dir = os.path.join(self.get_build_dir(), self.get_win_build_mode())
        # copy libs
        self.filter_copy(output_dir, os.path.join(self.get_install_dir(), 'lib'), '.lib')

        if self._project_settings.get_build_mode() == 'debug':
            src = os.path.join(self.get_install_dir(), 'lib', 'libprotobufd.lib')
            dst = os.path.join(self.get_install_dir(), 'lib', 'libprotobuf.lib')
            shutil.copy(src, dst);

        # copy exe
        self.filter_copy(output_dir, os.path.join(self.get_install_dir(), 'bin'), '.exe')

        # copy headers
        self.filter_copy(os.path.join(self.get_build_dir(), 'src'), os.path.join(self.get_install_dir(), 'include'),
                         '.h')
        self.filter_copy(os.path.join(self.get_build_dir(), 'src'), os.path.join(self.get_install_dir(), 'include'),
                         '.inc', False)

        # copy proto files
        self.filter_copy(os.path.join(self.get_build_dir(), 'src'), os.path.join(self.get_install_dir(), 'include'),
                         '.proto', False)
        return True

    def install_x(self):
        command = ['make', 'install']
        result = subprocess.call(command)
        if result != 0:
            print('Failed to install Protobuf')
            return False

        return True
