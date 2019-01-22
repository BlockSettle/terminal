stages {
        stage('Checkout') {
            steps {
                deleteDir()
                git branch: "${BUILD_BRANCH}",
                    credentialsId: 'terminal_build',
                    url: 'git@github.com:BlockSettle/terminal.git'
            }
        }
}
