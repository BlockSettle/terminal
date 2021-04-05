#
#
# ***********************************************************************************
# * Copyright (C) 2019 - 2020, BlockSettle AB
# * Distributed under the GNU Affero General Public License (AGPL v3)
# * See LICENSE or http://www.gnu.org/licenses/agpl.html
# *
# **********************************************************************************
#
#
import json
import os
import shutil
import subprocess
import sys
import subprocess
from datetime import date

from os import chdir, getcwd
from os.path import realpath

def StripQuotes(text):
   if text.startswith('"') and text.endswith('"'):
      return text[1:-1]

   return text

class PushdContext:
   cwd = None
   original_dir = None

   def __init__(self, dirname):
      self.cwd = os.path.realpath(dirname)

   def __enter__(self):
      self.original_dir = os.getcwd()
      os.chdir(self.cwd)
      return self

   def __exit__(self, type, value, tb):
      chdir(self.original_dir)

def pushd(dirname):
   return PushdContext(dirname)

def GetDigitRevisions(revisionString):
   revisions = revisionString.split('.')

   if len(revisions) != 3:
      raise 'Invalid revision string'

   return [ int(r) for r in revisions]

def ExtractCommitMessage(text):
   result = StripQuotes(text)
   if result.count(':') == 1:
      return result.split(':')[-1].strip()

   return result

def LoadGitCommits(repoPath, revisionFrom, revisionTo):
   result = []

   with pushd(repoPath) as ctx:
      command = ['git', 'log', '--pretty=format:"%s"', '--first-parent', '{}..{}'.format(revisionFrom, revisionTo)]
      result = subprocess.run(command, stdout=subprocess.PIPE)
      result = [ ExtractCommitMessage(m) for m in result.stdout.decode('utf-8').split('\n') ]

   return result

def GetCurrentGitRevision(repoPath):
   with pushd(repoPath) as ctx:
      result = subprocess.run(['git', 'log', '--pretty=format:"%h"', '-n', '1'], stdout=subprocess.PIPE)
      return StripQuotes(result.stdout.decode("utf-8"))

   return None

class Revision(object):
   def __init__(self, commonHash, terminalHash, revisionString, updates):
      self.commonHash = commonHash
      self.terminalHash = terminalHash
      self.revisionString = revisionString
      self.updates = updates

class Changelog(object):
   def __init__(self, changelogPath):
      self.changelogPath_ = changelogPath

      with open(self.changelogPath_, 'r') as f:
         self.content_ = json.load(f)

      return

   def GetLatestRevision(self):
      latestRevisionString = self.content_['latest_version']
      for c in self.content_['changes']:
         if c['version_string'] == latestRevisionString:
            return Revision(c['common_revision'], c['revision'], latestRevisionString, [])

      return None

   def SetLatestRevision(self, revision, releaseType):
      prevVersion = self.content_['latest_version']
      releaseDate = date.today().strftime('%d %b %Y')

      newRevision = {}

      newRevision['version_string'] = revision.revisionString
      newRevision['previous_version'] = prevVersion
      newRevision['release_date'] = releaseDate
      newRevision['release_type'] = releaseType
      newRevision['improvements'] = revision.updates
      newRevision['bug_fixes'] = []
      newRevision['revision'] = revision.terminalHash
      newRevision['common_revision'] = revision.commonHash

      self.content_['latest_version'] = revision.revisionString
      self.content_['release_date'] = releaseDate

      self.content_['changes'].insert(0, newRevision)

      self.SaveChangelog()


   def SaveChangelog(self):
      with open(self.changelogPath_, 'w') as f:
         json.dump(self.content_, f, indent=3)
      return

# /Users/user/Dev/BlocksettleTerminal/Deploy/Windows/bsterminal.nsi
# !define VERSION "xxx"
def UpdateWinRevision(filePath, revisionString):
   with open(filePath, 'r') as f:
      content = f.readlines()

   with open(filePath, 'w') as f:
      for l in content:
         if l.startswith('!define VERSION '):
            writeResult = f.write('!define VERSION "{}"\n'.format(revisionString))
            print('Windows installer version updated')
         else:
            writeResult = f.write(l)

# /Users/user/Dev/BlocksettleTerminal/Deploy/Ubuntu/DEBIAN/control
# Version: xxx
def UpdateLinuxRevision(filePath, revisionString):
   with open(filePath, 'r') as f:
      content = f.readlines()

   with open(filePath, 'w') as f:
      for l in content:
         if l.startswith('Version: '):
            writeResult = f.write('Version: {}\n'.format(revisionString))
            print('Linux installer version updated')
         else:
            writeResult = f.write(l)

# SET(BS_VERSION_MAJOR XXX )
# SET(BS_VERSION_MINOR XXX )
# SET(BS_VERSION_PATCH XXX )
def UpdateCmakeFile(filePath, revisionString):
   revisionDigits = GetDigitRevisions(revisionString)
   if len(revisionDigits) != 3:
      raise 'Invalid revision string'

   with open(filePath, 'r') as f:
      content = f.readlines()

   with open(filePath, 'w') as f:
      for l in content:
         if l.startswith('SET(BS_VERSION_MAJOR'):
            writeResult = f.write('SET(BS_VERSION_MAJOR {} )\n'.format(revisionDigits[0]))
         elif l.startswith('SET(BS_VERSION_MINOR'):
            writeResult = f.write('SET(BS_VERSION_MINOR {} )\n'.format(revisionDigits[1]))
         elif l.startswith('SET(BS_VERSION_PATCH'):
            writeResult = f.write('SET(BS_VERSION_PATCH {} )\n'.format(revisionDigits[2]))
         else:
            writeResult = f.write(l)


def UpdateToRevision(revisionString, releaseType):
   # load current changelog
   currentDir = os.getcwd()
   commonRepo = os.path.join(currentDir, 'common')

   cmakeFilePath = os.path.join(currentDir, 'CMakeLists.txt')
   changelogFilePath = os.path.join(currentDir, 'changelog.json')
   winInstallerFilePath = os.path.join(currentDir, 'Deploy', 'Windows', 'bsterminal.nsi')
   linuxInstallerFilePath = os.path.join(currentDir, 'Deploy', 'Ubuntu', 'DEBIAN', 'control')

   UpdateCmakeFile(cmakeFilePath, revisionString)
   UpdateWinRevision(winInstallerFilePath, revisionString)
   UpdateLinuxRevision(linuxInstallerFilePath, revisionString)

   changeLog = Changelog(changelogFilePath)

   currentRevision = GetCurrentGitRevision(currentDir)
   currentCommonRevision = GetCurrentGitRevision(commonRepo)

   latestRevisionObject = changeLog.GetLatestRevision()
   terminalCommitMessages = LoadGitCommits(currentDir, latestRevisionObject.terminalHash, currentRevision)
   commonCommitMessages = LoadGitCommits(commonRepo, latestRevisionObject.commonHash, currentCommonRevision)

   newRevision = Revision(currentCommonRevision, currentRevision, revisionString, terminalCommitMessages + commonCommitMessages)
   changeLog.SetLatestRevision(newRevision, releaseType)

   return

if __name__ == '__main__':
   if len(sys.argv) != 3:
      print('usage: revision release_type')
      exit(1)

   if sys.argv[2] != 'prod' and sys.argv[2] != 'dev':
      print('Pass prod or dev as release type')
      exit(1)

   UpdateToRevision(sys.argv[1], sys.argv[2])
