import multiprocessing
import os, sys, stat
import shutil
import subprocess

from component_configurator import Configurator


class MPIRSettings(Configurator):
    def __init__(self, settings):
        Configurator.__init__(self, settings)
        self._version = '3.0.0'
        self._package_name = 'mpir-' + self._version
        self._script_revision = '1'

        self._package_url = 'http://mpir.org/' + self._package_name + '.zip'

    def get_package_name(self):
        return self._package_name

    def get_revision_string(self):
        return self._version + self._script_revision

    def get_install_dir(self):
        return os.path.join(self._project_settings.get_common_build_dir(), 'mpir')

    def get_url(self):
        return self._package_url

    def is_archive(self):
        return True

    def config_windows(self):
        self.copy_sources_to_build()
        return True

    def get_solution_file(self):
        if self._project_settings.get_link_mode() == 'shared':
            return os.path.join(self.get_build_dir(), 'build.vc'
               + self._project_settings.get_vs_version_number(), 'dll_mpir_gc\\dll_mpir_gc.vcxproj')
        else:
            return os.path.join(self.get_build_dir(), 'build.vc'
               + self._project_settings.get_vs_version_number(), 'lib_mpir_gc\\lib_mpir_gc.vcxproj')

    def config_x(self):
        os.chmod(os.path.join(self.get_unpacked_sources_dir(), 'configure'),
                 stat.S_IEXEC + stat.S_IREAD + stat.S_IXGRP + stat.S_IRGRP)
        os.chmod(os.path.join(self.get_unpacked_sources_dir(), 'install-sh'),
                 stat.S_IEXEC + stat.S_IREAD + stat.S_IXGRP + stat.S_IRGRP)
        os.chmod(os.path.join(self.get_unpacked_sources_dir(), 'strip_fPIC.sh'),
                 stat.S_IEXEC + stat.S_IREAD + stat.S_IXGRP + stat.S_IRGRP)
        os.chmod(os.path.join(self.get_unpacked_sources_dir(), 'mpn', 'm4-ccas'),
                 stat.S_IEXEC + stat.S_IREAD + stat.S_IXGRP + stat.S_IRGRP)

        command = [os.path.join(self.get_unpacked_sources_dir(), 'configure'),
                   '--prefix',
                   self.get_install_dir(),
                   '--enable-gmpcompat']

        result = subprocess.call(command)
        return result == 0

    def make_windows(self):
        print('Making MPIR')

        command = ['msbuild',
                   self.get_solution_file(),
                   '/p:Configuration=' + self.get_win_build_mode(),
                   '/p:CL_MPCount=' + str(max(1, multiprocessing.cpu_count() - 1))]

        if self._project_settings.get_link_mode() == 'shared':
            command = ['msbuild',
                   self.get_solution_file(),
                   '/p:Configuration=' + self.get_win_build_mode(),
                   '/p:CL_MPCount=' + str(max(1, multiprocessing.cpu_count() - 1))]
                   #'/M:' + str(max(1, multiprocessing.cpu_count() - 1))]

        print('Running ' + ' '.join(command))

        result = subprocess.call(command)

        return result == 0

    def get_win_build_mode(self):
        if self._project_settings.get_build_mode() == 'release':
            return 'Release'
        else:
            return 'Debug'

    def get_win_platform(self):
        return 'x64'

    def make_x(self):
        command = ['make', '-j', str(multiprocessing.cpu_count())]

        result = subprocess.call(command)
        return result == 0

    def install_win(self):
        output_dir = os.path.join(self.get_build_dir(), 'lib', self.get_win_platform(), self.get_win_build_mode())
        # copy libs
        if self._project_settings.get_link_mode() == 'shared':
            output_dir = os.path.join(self.get_build_dir(), 'dll', self.get_win_platform(), self.get_win_build_mode())
            self.filter_copy(output_dir, os.path.join(self.get_install_dir(), 'lib'), '.dll')
            self.filter_copy(output_dir, os.path.join(self.get_install_dir(), 'lib'), '.lib', False)
        else:
            self.filter_copy(output_dir, os.path.join(self.get_install_dir(), 'lib'), '.lib')

        # copy headers
        self.filter_copy(output_dir, os.path.join(self.get_install_dir(), 'include'), '.h')

        return True

    def install_x(self):
        command = ['make', 'install']

        result = subprocess.call(command)
        return result == 0
