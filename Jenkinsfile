pipeline {
    agent any

    stages {
        stage('Build apps') {
            parallel {
            /*    stage('Build Linux app') {
                    agent {
                        docker {
                            image 'terminal:latest'
                            reuseNode true
                            args '-v /var/cache/3rd:/home/3rd'
                        }
                    }
                    steps {
                        sh "cd ./terminal && pip install requests"
                        sh "cd ./terminal && python generate.py release"
                        sh "cd ./terminal/terminal.release && make -j 16"
                        sh "cd ./terminal/Deploy && ./deploy.sh"
                    }
                }*/
                stage('Build MacOSX app') {
                    steps {
                        sh 'ssh Admin@10.1.60.206 "rm -rf ~/Workspace/terminal"'
                        sh 'ssh Admin@10.1.60.206 "cd ~/Workspace ; git clone --single-branch --branch ${TAG} git@github.com:BlockSettle/terminal.git ; git submodule init ; git submodule update"'
                  ///      sh "scp -r ${WORKSPACE}/terminal Admin@10.1.60.206:~/Workspace"
                  //      sh 'rsync -av --exclude terminal.release/ Admin@10.1.60.206:~/Workspace'
                        sh 'ssh Admin@10.1.60.206 "export PATH=/usr/local/opt/qt/bin:/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin ; ccache -s ; cd /Users/admin/Workspace/terminal/Deploy/MacOSX ; ./package.sh"'
                        sh "scp Admin@10.1.60.206:~/Workspace/terminal/Deploy/MacOSX/BlockSettle.dmg ${WORKSPACE}/terminal/Deploy/BlockSettle.dmg"
                    }
                }
            /*    stage('Build Windows app') {
                    steps {
                        sh 'ssh admin@172.17.0.1 -p2222 "del /s /f /q Workspace\\terminal"'
                        sh "scp -P 2222 -r ${WORKSPACE}/terminal admin@172.17.0.1:C:/Users/Admin/Workspace"
                        sh 'ssh admin@172.17.0.1 -p2222 "C:\\Users\\Admin\\Workspace\\build.bat"'
                        sh 'scp -P 2222 admin@172.17.0.1:C:/Users/Admin/Workspace/terminal/Deploy/bsterminal_installer.exe ${WORKSPACE}/terminal/Deploy/bsterminal_installer.exe'
                    }
                }*/
            }
        }
        
        stage('Transfer') {
            steps {
                sh "scp ${WORKSPACE}/terminal/Deploy/bsterminal.deb genoa@10.0.1.36:/var/www/terminal/Linux/bsterminal_${TAG}.deb"
                sh "ssh genoa@10.0.1.36 ln -sf /var/www/terminal/Linux/bsterminal_${TAG}.deb /var/www/downloads/bsterminal.deb"
                sh "scp ${WORKSPACE}/terminal/Deploy/BlockSettle.dmg genoa@10.0.1.36:/var/www/terminal/MacOSX/BlockSettle_${TAG}.dmg"
                sh "ssh genoa@10.0.1.36 ln -sf /var/www/terminal/MacOSX/BlockSettle_${TAG}.dmg /var/www/downloads/BlockSettle.dmg"
                sh "scp ${WORKSPACE}/terminal/Deploy/bsterminal_installer.exe genoa@10.0.1.36:/var/www/terminal/Windows/bsterminal_installer_${TAG}.exe"
                sh "ssh genoa@10.0.1.36 ln -sf /var/www/terminal/Windows/bsterminal_installer_${TAG}.exe /var/www/downloads/bsterminal_installer.exe"
            }
        }
    }
}
