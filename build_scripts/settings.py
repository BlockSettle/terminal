import os
import platform
import subprocess


class Settings:
    def __init__(self, build_mode, link_mode='static'):
        self._build_mode = build_mode
        self._link_mode = link_mode
        self._is_server_build = False

        self._project_root = os.getcwd()
        self._build_scripts_root = os.path.abspath(os.path.join(self._project_root, 'build_scripts'))
        self._root_dir = os.path.abspath(os.path.join(self._project_root, '..'))
        self._3rdparty_dir = os.getenv('DEV_3RD_ROOT', os.path.join(self._root_dir, '3rd'))
        download_dir_orig = os.path.join(self._3rdparty_dir, 'downloads')
        self._downloads_dir = os.getenv('DEV_3RD_DOWNLOADS', download_dir_orig)
        self._sources_dir = os.path.join(download_dir_orig, 'unpacked_sources')

        if link_mode == 'shared':
            self._common_build_dir = os.path.join(self._3rdparty_dir, build_mode + '-' + link_mode)
        else:
            self._common_build_dir = os.path.join(self._3rdparty_dir, build_mode)

        self._is_windows = False
        self._is_linux = False
        self._is_osx = False

        system_name = platform.system()
        if system_name == 'Windows':
            self._is_windows = True
        elif system_name == 'Darwin':
            self._is_osx = True
        elif system_name == 'Linux':
            self._is_linux = True
        else:
            raise EnvironmentError('System is not defined: ' + system_name)

        # create directory structure
        self.__create_directories__()

    def set_server_build_settings(self):
        self._is_server_build = True

    def is_server_build(self):
        return self._is_server_build

    def get_project_root(self):
        return self._project_root

    def get_build_scripts_root(self):
        return self._build_scripts_root

    def get_downloads_dir(self):
        return self._downloads_dir

    def get_common_build_dir(self):
        return self._common_build_dir

    def get_sources_dir(self):
        return self._sources_dir

    def get_build_mode(self):
        return self._build_mode

    def get_link_mode(self):
        return self._link_mode

    def on_windows(self):
        return self._is_windows

    def on_linux(self):
        return self._is_linux

    def on_osx(self):
        return self._is_osx

    def __create_if_not_exists__(self, directory):
        if not os.path.exists(directory):
            os.makedirs(directory)

    def __create_directories__(self):
        self.__create_if_not_exists__(self._3rdparty_dir)
        self.__create_if_not_exists__(self._downloads_dir)
        self.__create_if_not_exists__(self._common_build_dir)
        self.__create_if_not_exists__(self._sources_dir)

    def get_flags(self):
        build_mode = self.get_build_mode()
        if build_mode == 'release':
            return '-O3'
        elif build_mode == 'debug':
            return '-g'
        else:
            print("WARNING: using unknown build config : " + build_mode)
            return []

    def get_cmake_generator(self):
        if self._is_windows:
            return 'Visual Studio ' + self.get_vs_version_number() + ' Win64'
        else:
            return 'Unix Makefiles'

    def get_vs_year(self):
        return {
            '15': '2017',
            '14': '2015',
            '12': '2013'
        }.get(self.get_vs_version_number(), "NOT SUPPORTED VERSION")

    def get_vs_version_number(self):
        p = subprocess.Popen(['msbuild', '/version'], stdout=subprocess.PIPE)
        out, err = p.communicate()

        version = out.splitlines()[-1]
        if type(version) is type(b''):
            version = version.decode('utf-8')

        return version.split('.')[0]
