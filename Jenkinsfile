pipeline {
    agent any
    options {
        lock resource: 'terminal_lock'
    }

    stages {
        stage('Build apps') {
            parallel {
                stage('Build Linux app') {
                    agent {
                        docker {
                            image 'terminal:latest'
                            reuseNode true
                            args '-v /var/cache/3rd/downloads:${WORKSPACE}/3rd/downloads'
                            args '-v /var/cache/3rd/release:${WORKSPACE}/3rd/release'
                        }
                    }
                    steps {
                        sh "cd ./terminal && pip install requests"
                        sh "cd ./terminal && python generate.py release --production"
                        sh "cd ./terminal/terminal.release && make -j 4"
                        sh "cd ./terminal/Deploy && ./deploy.sh"
                    }
                }
                stage('Build MacOSX app') {
                    steps {
                        sh 'ssh ${MACOS_HOST_USER}@${MACOS_HOST_IP} "rm -rf ~/Workspace/terminal"'
                        sh 'ssh ${MACOS_HOST_USER}@${MACOS_HOST_IP} "cd ~/Workspace ; git clone --single-branch --branch ${TAG} https://github.com/BlockSettle/terminal.git ; cd terminal ; git submodule init ; git submodule update ; cd common ; git submodule init ; git submodule update"'
                        sh 'ssh ${MACOS_HOST_USER}@${MACOS_HOST_IP} "export PATH=/Users/${MACOS_HOST_USER}/.pyenv/shims:/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin; ccache -s ; cd /Users/${MACOS_HOST_USER}/Workspace/terminal/Deploy/MacOSX ; security unlock-keychain -p ${MAC_CHAIN_PAS} login.keychain ; ./package.sh -production"'
                        sh "scp ${MACOS_HOST_USER}@${MACOS_HOST_IP}:~/Workspace/terminal/Deploy/MacOSX/BlockSettle.dmg ${WORKSPACE}/terminal/Deploy/BlockSettle.dmg"
                    }
                }
                stage('Build Windows app') {
                    agent {
                        label 'windows'
                    }
                    steps {
                        bat "cd terminal\\common && git submodule update"
                        bat 'set DEV_3RD_ROOT=C:\\Jenkins\\workspace\\3rd&& "C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat" && cd terminal && python generate.py release --production'
                        bat '"C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat" && cd terminal\\terminal.release && devenv BS_Terminal.sln /build RelWithDebInfo"'
                        bat "cd terminal\\Deploy\\Windows\\ && deploy.bat"
                    }
                }
            }
        }

        stage('Transfer') {
            steps {
                sh "scp ${WORKSPACE}/terminal/Deploy/bsterminal.deb genoa@10.0.1.36:/var/www/terminal/Linux/bsterminal_${TAG}.deb"
                sh "ssh genoa@10.0.1.36 ln -sf /var/www/terminal/Linux/bsterminal_${TAG}.deb /var/www/downloads/bsterminal.deb"
                sh "scp ${WORKSPACE}/terminal/Deploy/BlockSettle.dmg genoa@10.0.1.36:/var/www/terminal/MacOSX/BlockSettle_${TAG}.dmg"
                sh "ssh genoa@10.0.1.36 ln -sf /var/www/terminal/MacOSX/BlockSettle_${TAG}.dmg /var/www/downloads/BlockSettle.dmg"
                sh 'scp -p2222 admin@10.0.1.135:C:/Jenkins/workspace/terminal/terminal/Deploy/bsterminal_installer.exe ${WORKSPACE}/terminal/Deploy/bsterminal_installer.exe'
                sh "scp ${WORKSPACE}/terminal/Deploy/bsterminal_installer.exe genoa@10.0.1.36:/var/www/terminal/Windows/bsterminal_installer_${TAG}.exe"
                sh "ssh genoa@10.0.1.36 ln -sf /var/www/terminal/Windows/bsterminal_installer_${TAG}.exe /var/www/downloads/bsterminal_installer.exe"
            }
        }
        stage('Upload changelog') {
            steps {
                sh "scp ${WORKSPACE}/terminal/changelog.json genoa@10.0.1.36:/var/www/Changelog/changelog.json"
            }
        }
    }
}
