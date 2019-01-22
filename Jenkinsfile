pipeline {
    agent any

    stages {
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
