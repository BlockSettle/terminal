import dmgbuild
import json
import multiprocessing
import os
import platform
import shutil
import subprocess
import sys

def generate_project(sourcesRoot, productionBuild):
   command = []

   command.append('python')
   command.append('generate.py')
   command.append('release')

   if productionBuild:
      command.append('-production')

   result = subprocess.call(command, cwd=sourcesRoot)
   return result == 0

def make_project(sourcesRoot):
   wd = os.path.abspath(os.path.join(sourcesRoot, 'terminal.release'))

   command = []

   command.append('make')
   command.append('-j')
   command.append( str(multiprocessing.cpu_count()) )

   result = subprocess.call(command, cwd=wd)
   return result == 0

def sign_single_app(appPath):

   command = []

   command.append('codesign')
   command.append('-s')
   command.append('Developer ID Application: BlockSettle AB (Q47AVPUL6K)')
   command.append(appPath)

   print(' '.join(command))

   return subprocess.call(command) == 0

def check_signature(appPath):

   command = 'codesign -v -R="anchor trusted" "{}"'.format(appPath)

   print(command)

   return subprocess.call(command, shell=True) == 0

def sign_apps(sourcesRoot):
   buildDir = os.path.abspath(os.path.join(sourcesRoot, 'build_terminal', 'RelWithDebInfo', 'bin'))

   apps = ['BlockSettle Terminal.app', 'BlockSettle Signer.app']

   for appName in apps:
      appPath = os.path.join(buildDir, appName)
      if sign_single_app(appPath) and check_signature(appPath):
         print('Signed: {}'.format(appName))
      else:
         print('Failed to sign: {}'.format(appName))
         return False

   return True

def get_package_path(outputDir):
   return os.path.join(outputDir, 'BlockSettle.dmg')

def make_package(sourcesRoot, packagePath):
   buildDir = os.path.abspath(os.path.join(sourcesRoot, 'build_terminal', 'RelWithDebInfo'))
   binDir = os.path.join(buildDir, 'bin')
   packgeDir = os.path.join(buildDir, 'Blocksettle')

   if os.path.isdir(packgeDir):
      shutil.rmtree(packgeDir)
   elif os.path.exists(packgeDir):
      os.remove(packgeDir)

   if os.path.exists(packagePath):
      os.remove(packagePath)

   os.rename(binDir, packgeDir)
   pkgSettings = {
      "title": "Blocksettle Terminal",
      "background": "builtin-arrow",
      "format": "UDZO",
      "compression-level": 9,
      "window": { "position": { "x": 100, "y": 100 },
      "size": { "width": 640, "height": 280 } },
      "contents": [
         { "x": 140, "y": 120, "type": "file","path": packgeDir},
         { "x": 500, "y": 120, "type": "link", "path": "/Applications" }
      ]
   }

   shutil.copyfile(os.path.join(sourcesRoot, 'Scripts', 'RFQBot.qml'), os.path.join(packgeDir, 'RFQBot.qml'))

   settingsFile = 'settings.json'

   if os.path.exists(settingsFile):
      os.remove(settingsFile)

   with open(settingsFile, 'w') as outfile:
      json.dump(pkgSettings, outfile)

   dmgbuild.build_dmg(packagePath, 'Blocksettle', settingsFile)
   if os.path.exists(packagePath):
      print('Package created: {}'.format(packagePath))
      os.remove(settingsFile)
      return True
   else:
      print('Failed to create PKG file with settings: {}'.format(settingsFile))
      return False

def sign_package(packagePath):
   if not (sign_single_app(packagePath) and check_signature(packagePath)):
      print('Failed to sign package')
      return False

   return True

def main(sourcesRoot, productionBuild):
   outputDir = os.getcwd()

   if generate_project(sourcesRoot, productionBuild):
      if make_project(sourcesRoot) and sign_apps(sourcesRoot):
         packagePath = os.path.join(outputDir, 'BlockSettle.dmg')
         if make_package(sourcesRoot, packagePath) and sign_package(packagePath):
            print('Generated package saved to {}'.format(packagePath))
         else:
            print('Failed to make package')
      else:
         print('Failed to make project')
   else:
      print('Failed to generate project')

if __name__ == '__main__':
   if len(sys.argv) < 2:
      print('Pass sources root')
   else:
      if (len(sys.argv) == 3) and (sys.argv[2] == '-production'):
         main(sys.argv[1], True)
      else:
         main(sys.argv[1], False)
