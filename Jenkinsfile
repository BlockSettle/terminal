pipeline {
    agent any

    stages {
        stage('Checkout') {
            steps {
                deleteDir()

                dir("terminal") {
                    git branch: "ci-test",
                        credentialsId: 'terminal_build',
                        url: 'git@github.com:BlockSettle/terminal.git'
                    
                dir("common") {
                    git branch: "bs_dev",
                        credentialsId: 'terminal_build',
                        url: 'git@github.com:BlockSettle/common.git'
                }
                dir("AuthCommon") {
                    git branch: "master",
                        credentialsId: 'terminal_build',
                        url: 'git@github.com:scomil/AuthCommon.git'
                }
                dir("Celer") {
                    git branch: "master",
                        credentialsId: 'terminal_build',
                        url: 'git@github.com:BlockSettle/Celer.git'
                }
                }
            }
        }

        stage('Build app') {
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
        }
        
        stage('Transfer') {
            steps {
                sh "scp ${WORKSPACE}/terminal/Deploy/bsterminal.deb genoa@10.0.1.36:/var/www/downloads/builds/Linux"
            //    sh "ssh genoa@10.0.1.36 ln -sf /var/www/downloads/builds/Linux /var/www/downloads/latests"
            }
        }
    }
}
