#
#
# ***********************************************************************************
# * Copyright (C) 2011 - 2020, BlockSettle AB
# * Distributed under the GNU Affero General Public License (AGPL v3)
# * See LICENSE or http://www.gnu.org/licenses/agpl.html
# *
# **********************************************************************************
#
#
import argparse
import os
import shutil
import subprocess
import sys

# Set the minimum macOS target environment. Applies to prereqs and to BS code.
# If the min target changes, update CMakeLists.txt too.
if sys.platform == "darwin":
   os.environ['MACOSX_DEPLOYMENT_TARGET'] = '10.12'

sys.path.insert(0, os.path.join('common'))
sys.path.insert(0, os.path.join('common', 'build_scripts'))

from build_scripts.bip_protocols_settings       import BipProtocolsSettings
from build_scripts.botan_settings               import BotanSettings
from build_scripts.gtest_settings               import GtestSettings
from build_scripts.hidapi_settings              import HidapiSettings
from build_scripts.jom_settings                 import JomSettings
from build_scripts.libbtc_settings              import LibBTC
from build_scripts.libchacha20poly1305_settings import LibChaCha20Poly1305Settings
from build_scripts.libqrencode_settings         import LibQREncode
from build_scripts.libusb_settings              import LibusbSettings
from build_scripts.mpir_settings                import MPIRSettings
from build_scripts.nlohmann_json_settings       import NLohmanJson
from build_scripts.openssl_settings             import OpenSslSettings
from build_scripts.protobuf_settings            import ProtobufSettings
from build_scripts.qt_settings                  import QtSettings
from build_scripts.settings                     import Settings
from build_scripts.spdlog_settings              import SpdlogSettings
from build_scripts.trezor_common_settings       import TrezorCommonSettings
from build_scripts.websockets_settings          import WebsocketsSettings
from build_scripts.zeromq_settings              import ZeroMQSettings

def generate_project(build_mode, link_mode, build_production, hide_warnings, cmake_flags, build_tests, build_tracker):
   project_settings = Settings(build_mode, link_mode)

   print('Build mode        : {} ( {} )'.format(project_settings.get_build_mode(), ('Production' if build_production else 'Development')))
   print('Build mode        : ' + project_settings.get_build_mode())
   print('Link mode         : ' + project_settings.get_link_mode())
   print('CMake generator   : ' + project_settings.get_cmake_generator())
   print('Download path     : ' + os.path.abspath(project_settings.get_downloads_dir()))
   print('Install dir       : ' + os.path.abspath(project_settings.get_common_build_dir()))

   required_3rdparty = []
   if project_settings._is_windows:
      required_3rdparty.append(JomSettings(project_settings))

   required_3rdparty += [
      ProtobufSettings(project_settings),
      OpenSslSettings(project_settings),
      SpdlogSettings(project_settings),
      ZeroMQSettings(project_settings),
      LibQREncode(project_settings),
      MPIRSettings(project_settings),
      LibBTC(project_settings),                             # static
      LibChaCha20Poly1305Settings(project_settings),        # static
      WebsocketsSettings(project_settings),
      BotanSettings(project_settings),
      QtSettings(project_settings),
      HidapiSettings(project_settings),
      LibusbSettings(project_settings),
      TrezorCommonSettings(project_settings),
      BipProtocolsSettings(project_settings),
      NLohmanJson(project_settings)
   ]

   if build_tests:
      required_3rdparty.append(GtestSettings(project_settings))

   for component in required_3rdparty:
      if not component.config_component():
         print('FAILED to build ' + component.get_package_name() + '. Cancel project generation')
         return 1

   print('3rd party components ready')
   print('Start generating project')
   os.chdir(project_settings.get_project_root())

   generated_dir = os.path.join(os.getcwd(), 'generated_proto')
   if link_mode == 'shared':
      build_dir = os.path.join(os.getcwd(), 'terminal.' + build_mode + '-' + link_mode)
   else:
      build_dir = os.path.join(os.getcwd(), 'terminal.' + build_mode)

   if os.path.isfile(generated_dir):
      os.remove(generated_dir)
   elif os.path.isdir(generated_dir):
      shutil.rmtree(generated_dir)

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
   if project_settings._is_windows:
      command.append('-A x64 ')
   command.append('-DCMAKE_CURRENT_SOURCE_DIR=..')
   if build_mode == 'debug':
      command.append('-DCMAKE_BUILD_TYPE=Debug')
      if project_settings._is_windows:
         command.append('"-DCMAKE_CXX_FLAGS_DEBUG=/D_DEBUG -DBUILD_MT_RELEASE=OFF /MTd /Zi /Ob0 /Od /RTC1"')
         command.append('-DCMAKE_CONFIGURATION_TYPES=Debug')
   else:
      command.append('-DCMAKE_BUILD_TYPE=RelWithDebInfo')
      if project_settings._is_windows:
         command.append('"-DCMAKE_CXX_FLAGS_RELEASE=/MT /O2 /Ob2 /DNDEBUG"')
         command.append('"-DCMAKE_CXX_FLAGS_RELWITHDEBINFO=/MT /O2 /Ob2 /DNDEBUG"')
         command.append('-DCMAKE_CONFIGURATION_TYPES=RelWithDebInfo')

   if build_production:
      command.append('-DPRODUCTION_BUILD=1')

   if project_settings.get_link_mode() == 'shared':
      command.append('-DBSTERMINAL_SHARED_LIBS=ON')

   # to remove cmake 3.10 dev warnings
   command.append('-Wno-dev')

   if build_tests:
      command.append('-DBUILD_TESTS=1')

   if build_tracker:
      command.append('-DBUILD_TRACKER=1')

   if cmake_flags != None:
      for flag in cmake_flags.split():
         command.append(flag)

   if project_settings._is_windows:
      cmdStr = r' '.join(command)
      result = subprocess.call(cmdStr)
   else:
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
   input_parser.add_argument('--production',
                             help='Make production build',
                             action='store_true',
                             dest='build_production',
                             default=False)
   input_parser.add_argument('link_mode',
                             help='Linking library type used by the project generator [ static | shared ]',
                             nargs='?',
                             action='store',
                             default='static',
                             choices=['static', 'shared'])
   input_parser.add_argument('--hide-warnings',
                             help='Hide warnings in external sources',
                             action='store_true',
                             dest='hide_warnings',
                             default=False)
   input_parser.add_argument('--cmake-flags',
                             action='store',
                             type=str,
                             help='Additional CMake flags. Example: "-DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_FLAGS=-fuse-ld=gold"')
   input_parser.add_argument('--test',
                             help='Select to also build tests',
                             action='store_true')
   input_parser.add_argument('--tracker',
                             help='Select to also build tracker',
                             action='store_true')

   args = input_parser.parse_args()

   sys.exit(generate_project(args.build_mode, args.link_mode, args.build_production, args.hide_warnings, args.cmake_flags, args.test, args.tracker))
