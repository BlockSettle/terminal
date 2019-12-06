import os
import requests
import shutil
import tarfile
import zipfile
import subprocess

requests.packages.urllib3.disable_warnings()


class Configurator:
    def __init__(self, project_settings):
        self._project_settings = project_settings

    def config_component(self):
        if self.build_required():
            print('Start building : {}'.format(self.get_package_name()))
            if self.download_package():
                if self.is_archive():
                    self.unpack_package()
                build_dir = self.get_build_dir()
                self.remove_fs_object(build_dir)

                os.makedirs(build_dir)

                os.chdir(build_dir)

                if os.path.isdir(self.get_install_dir()):
                    self.remove_fs_object(self.get_install_dir())

                if self.config() and self.make() and self.install():
                    self.SetRevision()
                    return True

                return False
            return False
        return True

    def GetRevisionFileName(self):
        return os.path.join(self.get_install_dir(), '3rd_revision.txt')

    def RevisionUpToDate(self):
        revisionFileName = self.GetRevisionFileName()
        if os.path.isfile(revisionFileName) :
            with open(revisionFileName, 'r') as f:
                revisionData = f.read()
            return revisionData == self.get_revision_string()

        return False

    def SetRevision(self):
        revisionFileName = self.GetRevisionFileName()
        with open(revisionFileName, 'w') as f:
            f.write(self.get_revision_string())

    def download_package(self):
        url = self.get_url()
        ext = url.split('.')[-1]
        if url.endswith('.tar.gz'):
            ext = 'tar.gz'
        elif url.endswith('.tar.xz'):
            ext = 'tar.xz'

        print('Start download : {}'.format(url))

        self._file_name = self.get_package_name() + '.' + ext
        self._download_path = os.path.join(self._project_settings.get_downloads_dir(), self._file_name)

        if url.endswith('.xz') and os.path.isfile(self._download_path[:-3]):
            return True

        if not os.path.isfile(self._download_path):
            req = requests.get(url, stream=True)
            req.raw.decode_content = True
            with open(self._download_path, 'wb+') as save_file:
                shutil.copyfileobj(req.raw, save_file)

        print('\nDownloaded: ' + self.get_package_name())

        return True

    def unpack_package(self):
        if self._file_name.endswith('.zip'):
            extension = '.zip'
            extractor = zipfile.ZipFile(self._download_path, 'r')
        elif self._file_name.endswith('.tar.gz'):
            extension = '.tar.gz'
            extractor = tarfile.open(self._download_path, 'r:gz')
        elif self._file_name.endswith('.tar.xz'):
            tarpath = self._download_path[:-3]
            if not os.path.isfile(self._download_path[:-3]):
                command = ['unxz', self._download_path]
                result = subprocess.call(command)
                if result != 0:
                    raise ValueError('Call to unxz failed')

            extractor = tarfile.open(tarpath, 'r')
            extension = '.tar.xz'
        else:
            raise ValueError('Could not get extraction path for ' + self._file_name)

        if not hasattr(self, '_package_dir_name'):
           self._package_dir_name = self._file_name[:-(len(extension))]
        if not os.path.isdir(self.get_unpacked_sources_dir()):
            print('Start unpacking: ' + self.get_package_name())
            try:
                if self.unpack_in_common_dir():
                    if self._project_settings.on_windows():
                        extractor.extractall('\\\\?\\' + self._project_settings.get_sources_dir())
                    else:
                        extractor.extractall(self._project_settings.get_sources_dir())
                else:
                    if self._project_settings.on_windows():
                        extractor.extractall('\\\\?\\' + self.get_unpacked_sources_dir())
                    else:
                        extractor.extractall(self.get_unpacked_sources_dir())
            except:
                print("unpacking exception")

    # some packages will be unpacked to individual directory by default
    # if it is not happening - overload this function and return False, directory will be created
    def unpack_in_common_dir(self):
        return True

    def build_required(self):
        if os.path.isdir(self.get_install_dir()):
            return not self.RevisionUpToDate()

        return True

    def get_install_dir(self):
        return os.path.join(self._project_settings.get_common_build_dir(), self.get_package_name())

    def get_unpacked_sources_dir(self):
        return os.path.join(self._project_settings.get_sources_dir(), self._package_dir_name)

    def get_build_dir(self):
        return os.path.join(self._project_settings.get_sources_dir(), 'build_' + self._package_dir_name)

    def config(self):
        if self._project_settings.on_windows():
            result = self.config_windows()
        else:
            result = self.config_x()

        if not result:
            print('Failed to config')
        return result

    def make(self):
        if self._project_settings.on_windows():
            result = self.make_windows()
        else:
            result = self.make_x()

        if not result:
            print('Failed to make')
        return result

    def install(self):
        if self._project_settings.on_windows():
            result = self.install_win()
        else:
            result = self.install_x()

        if not result:
            print('Failed to install')
        return result

    def copy_sources_to_build(self):
        print('Copy unpacked sources to build directory for: ' + self.get_package_name())
        src = self.get_unpacked_sources_dir()
        dst = self.get_build_dir()

        for name in os.listdir(src):
            src_name = os.path.join(src, name)
            dst_name = os.path.join(dst, name)

            if os.path.isdir(src_name):
                shutil.copytree(src_name, dst_name)
            else:
                shutil.copy(src_name, dst_name)

    def remove_fs_object(self, name):
        if os.path.isfile(name):
            os.remove(name)
        elif os.path.isdir(name):
            shutil.rmtree(name)

    def filter_copy(self, src, dst, file_extension=None, cleanupDst=True):
        if cleanupDst:
            self.remove_fs_object(dst)

        if not os.path.isdir(dst):
            os.makedirs(dst)

        for name in os.listdir(src):
            src_name = os.path.join(src, name)
            dst_name = os.path.join(dst, name)

            if os.path.isfile(src_name):
                if not file_extension or src_name.endswith(file_extension):
                    shutil.copy(src_name, dst_name)
            else:
                self.filter_copy(src_name, dst_name, file_extension, cleanupDst)
