import multiprocessing
import os
import subprocess

from component_configurator import Configurator


class gRPCSettings(Configurator):
    def __init__(self, settings):
        Configurator.__init__(self, settings)
        self._version = '1.25.0'
        self._package_name = 'grpc-' + self._version + '-2'
        self._package_dir_name = 'gRPC'

    def get_package_name(self):
        return self._package_name

    def get_revision_string(self):
        return self._package_name

    def get_install_dir(self):
        return os.path.join(self._project_settings.get_common_build_dir(), 'gRPC')

    def config_component(self):
        if self.build_required():
            download_dir = os.path.join(self._project_settings.get_downloads_dir(), 'unpacked_sources', 'gRPC')
            if not os.path.exists(download_dir):
                os.makedirs(download_dir)
            os.chdir(download_dir)
            
            subprocess.call(['git', 'clone', 'https://github.com/grpc/grpc', '.'])
            subprocess.check_call(['git', 'fetch'])
            subprocess.check_call(['git', 'checkout', 'v' + self._version])
            subprocess.check_call(['git', 'checkout', '.'])
            subprocess.check_call(['git', 'submodule', 'update', '--init', '--recursive'])
            
            build_dir = self.get_build_dir()
            self.remove_fs_object(build_dir)
            os.makedirs(build_dir)
            os.chdir(build_dir)

            if self.config() and self.make() and self.install():
                self.SetRevision()
                return True
            else:
                return False
        return True

    def config_x(self):
        command = ['cmake',
            os.path.join(self.get_unpacked_sources_dir()),
            '-G', self._project_settings.get_cmake_generator(),
            '-DCMAKE_INSTALL_PREFIX=' + self.get_install_dir(),
        ]

        result = subprocess.check_call(command)
        return True

    def make_windows(self):
        return False

    def make_x(self):
        command = ['make', '-j', str(multiprocessing.cpu_count())]
        result = subprocess.check_call(command)
        return result == 0

    def install_win(self):
        # not tested
        result = subprocess.check_call('--build', './', '--config', 'Debug', '--target', 'INSTALL')
        return True

    def install_x(self):
        command = ['make', 'install']
        result = subprocess.check_call(command)
        self.filter_copy('.', os.path.join(self.get_install_dir(), 'lib'), file_extension='.a')
        self.filter_copy('.', os.path.join(self.get_install_dir(), 'bin'), file_extension='plugin')
        return result == 0
