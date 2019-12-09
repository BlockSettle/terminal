import argparse
import multiprocessing
import os
import platform
import shutil
import subprocess
import sys
sys.path.insert(0, os.path.join('terminalGUI', 'common'))
sys.path.insert(0, os.path.join('terminalGUI', 'common', 'build_scripts'))

from build_scripts.gtest_settings         import GtestSettings
from build_scripts.jom_settings           import JomSettings
from build_scripts.libbtc_settings        import LibBTC
from build_scripts.libchacha20poly1305_settings import LibChaCha20Poly1305Settings
from build_scripts.mpir_settings          import MPIRSettings
from build_scripts.openssl_settings       import OpenSslSettings
from build_scripts.protobuf_settings      import ProtobufSettings
from build_scripts.libqrencode_settings   import LibQREncode
from build_scripts.qt_settings            import QtSettings
from build_scripts.settings               import Settings
from build_scripts.spdlog_settings        import SpdlogSettings
from build_scripts.websockets_settings    import WebsocketsSettings
from build_scripts.zeromq_settings        import ZeroMQSettings
from build_scripts.botan_settings         import BotanSettings


gui_build_path = ''

def generate_terminal_GUI(build_mode, cmake_flags):
   project_settings = Settings(build_mode)
   cur_dir = os.getcwd()

   if not os.path.isdir('terminalGUI'):
      print("terminalGUI submodule doesn't exist")
      return False

   global gui_build_path
   if project_settings._is_windows:
      if project_settings.get_build_mode() == 'debug':
         gui_build_path = os.path.join('terminalGUI', 'build_terminal_GUI', 'Debug', 'bin', 'Debug')
      else:
         gui_build_path = os.path.join('terminalGUI', 'build_terminal_GUI', 'Release', 'bin', 'Release')
   else:
      if project_settings.get_build_mode() == 'debug':
         gui_build_path = os.path.join('terminalGUI', 'build_terminal_GUI', 'Debug', 'libs')
      else:
         gui_build_path = os.path.join('terminalGUI', 'build_terminal_GUI', 'Release', 'libs')

   if os.path.isdir(gui_build_path):
      print("terminalGUI already built")
      return True

   os.chdir('terminalGUI')


   if os.getenv('DEV_3RD_ROOT') == '':
      top_path = os.path.abspath(os.path.join(cur_dir, '..'))
      os.environ['DEV_3RD_ROOT'] = os.path.join(top_path, '3rd')

   command = []

   command.append('python')
   command.append('generate.py')
   command.append('-test')
   command.append(build_mode)

   if cmake_flags != None:
      for flag in cmake_flags.split():
         command.append(flag)

   result = subprocess.call(command)
   if result == 0:
      print('terminalGUI generation succeeded')

      os.chdir('terminalGUI.' + build_mode)

      if project_settings._is_windows:
         build_conf = 'Debug'
         if project_settings.get_build_mode() == 'release':
            build_conf = 'RelWithDebInfo'

         command = ['msbuild', 'BS_Terminal_GUI.sln',
                   '/p:Configuration=' + build_conf,
                   '/p:CL_MPCount=' + str(max(1, multiprocessing.cpu_count() - 1))]

         print(' '.join(command))

         result = subprocess.call(command)
         if result == 0:
            print("terminalGUI build succeeded")
         else:
            return False

      else:
         command = ['make', '-j', str(multiprocessing.cpu_count())]

         result = subprocess.call(command)
         if result == 0:
            print("terminalGUI build succeeded")
         else:
            return False

      os.chdir(cur_dir)
      return True

   else:
      print('terminalGUI project generation failed')
      return False


def generate_project(build_mode, cmake_flags):
   project_settings = Settings(build_mode)

   print('Checking 3rd party components')
   print('Build mode        : ' + project_settings.get_build_mode())
   print('CMake generator   : ' + project_settings.get_cmake_generator())
   print('Download path     : ' + project_settings.get_downloads_dir())
   print('Install dir       : ' + project_settings.get_common_build_dir())

   required_3rdparty = []
   if project_settings._is_windows:
      required_3rdparty.append(JomSettings(project_settings))

   required_3rdparty += [
      ProtobufSettings(project_settings),
      OpenSslSettings(project_settings),
      QtSettings(project_settings),
      SpdlogSettings(project_settings),
      ZeroMQSettings(project_settings),
      MPIRSettings(project_settings),
      LibBTC(project_settings),
      LibChaCha20Poly1305Settings(project_settings),
      WebsocketsSettings(project_settings),
      BotanSettings(project_settings),
      LibQREncode(project_settings),
      GtestSettings(project_settings)
      ]

   for component in required_3rdparty:
      if not component.config_component():
         print('FAILED to build ' + component.get_package_name() + '. Cancel project generation')
         return 1

   print('3rd party components ready')
   print('Start generating project')
   os.chdir(project_settings.get_project_root())

   build_dir = os.path.join(os.getcwd(), 'tests.' + build_mode)

   if os.path.isfile(build_dir):
      os.remove(build_dir)
   elif os.path.isdir(build_dir):
      shutil.rmtree(build_dir)

   os.makedirs(build_dir)
   os.chdir(build_dir)

   command = []

   command.append('cmake')
   command.append('..')
   command.append('-G')
   command.append( project_settings.get_cmake_generator())
   if build_mode == 'debug':
      command.append('-DCMAKE_BUILD_TYPE=Debug')
      if project_settings._is_windows:
         command.append('-DCMAKE_CXX_FLAGS_DEBUG=/D_DEBUG /MTd /Zi /Ob0 /Od /RTC1')
         command.append('-DCMAKE_CONFIGURATION_TYPES=Debug')
   else:
      command.append('-DCMAKE_BUILD_TYPE=Release')
      if project_settings._is_windows:
         command.append('-DCMAKE_CXX_FLAGS_RELEASE=/MT /O2 /Ob2 /DNDEBUG')
         command.append('-DCMAKE_CONFIGURATION_TYPES=Release')

   command.append('-DBS_GUI_BUILD_PATH=' + os.path.join('..', gui_build_path))


   # to remove cmake 3.10 dev warnings
   command.append('-Wno-dev')

   if cmake_flags != None:
      for flag in cmake_flags.split():
         command.append(flag)

   result = subprocess.call(command)
   if result == 0:
      print('Project generated to :' + build_dir)
      return 0
   else:
      print('Cmake failed')
      return 1

if __name__ == '__main__':
   input_parser = argparse.ArgumentParser()
   input_parser.add_argument('build_mode',
                             help='Build mode to be used by the project generator [ debug | release ]',
                             nargs='?',
                             action='store',
                             default='release',
                             choices=['debug', 'release'])
   input_parser.add_argument('-cmake-flags',
                             action='store',
                             type=str,
                             help='Additional CMake flags. Example: "-DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_FLAGS=-fuse-ld=gold"')

   args = input_parser.parse_args()

   if not generate_terminal_GUI(args.build_mode, args.cmake_flags):
      sys.exit(1)

   sys.exit(generate_project(args.build_mode, args.cmake_flags))
