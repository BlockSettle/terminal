import multiprocessing
import os
import shutil
import subprocess

from component_configurator import Configurator

class ProtobufSettings(Configurator):
   def __init__(self, settings):
      Configurator.__init__(self, settings)
      self._version = '3.5.2'
      self._package_name = 'protobuf-' + self._version

      self._package_url = 'https://github.com/google/protobuf/archive/v' + self._version + '.tar.gz'

   def get_package_name(self):
      return self._package_name

   def get_url(self):
      return self._package_url

   def get_install_dir(self):
      return os.path.join(self._project_settings.get_common_build_dir(), 'Protobuf')

   def is_archive(self):
      return True

   def config_windows(self):
      self.copy_sources_to_build()

      print('Generating protobuf solution')

      command = ['cmake', os.path.join(self.get_unpacked_sources_dir(), 'cmake'), '-G']
      command.append(self._project_settings.get_cmake_generator())
      command.append('-Dprotobuf_BUILD_TESTS=OFF')
      command.append('-Dprotobuf_MSVC_STATIC_RUNTIME=ON');

      result = subprocess.call(command)
      return result == 0

   def get_solution_file(self):
      return os.path.join( self.get_build_dir(), 'protobuf.sln')

   def config_x(self):
      cwd = os.getcwd()
      os.chdir(self.get_unpacked_sources_dir())
      command = ['./autogen.sh']
      result = subprocess.call(command)
      if result != 0:
         return False

      os.chdir(cwd)
      command = [os.path.join(self.get_unpacked_sources_dir(), 'configure')]

      command.append('--prefix')
      command.append(self.get_install_dir())

      result = subprocess.call(command)
      return result == 0

   def make_windows(self):
      print('Making protobuf: might take a while')
      command = []

      command.append('devenv')
      command.append(self.get_solution_file())
      command.append('/build')
      command.append(self.get_win_build_mode())
      command.append('/project')
      command.append('protoc')

      result = subprocess.call(command)
      return result == 0

   def get_win_build_mode(self):
      if self._project_settings.get_build_mode() == 'release':
         return 'Release'
      else:
         return 'Debug'

   def make_x(self):
      command = []

      command.append('make')
      command.append('-j')
      command.append( str(multiprocessing.cpu_count()) )

      result = subprocess.call(command)
      return result == 0

   def install_win(self):
      output_dir = os.path.join( self.get_build_dir(), self.get_win_build_mode())
      # copy libs
      self.filter_copy(output_dir, os.path.join(self.get_install_dir(), 'lib'), '.lib' )

      if self._project_settings.get_build_mode() == 'debug':
         src = os.path.join(self.get_install_dir(), 'lib', 'libprotobufd.lib')
         dst = os.path.join(self.get_install_dir(), 'lib', 'libprotobuf.lib')
         shutil.copy(src, dst);

      # copy exe
      self.filter_copy(output_dir, os.path.join(self.get_install_dir(), 'bin'), '.exe' )

      # copy headers
      self.filter_copy(os.path.join(self.get_build_dir(), 'src'), os.path.join(self.get_install_dir(), 'include'), '.h')

      return True

   def install_x(self):
      command = ['make', 'install']
      result = subprocess.call(command)
      if result != 0:
         print('Failed to install Protobuf')
         return False

      return True
