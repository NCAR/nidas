pipeline {

  agent none

  options {
    buildDiscarder(
      logRotator(
        artifactDaysToKeepStr: '28',
        daysToKeepStr: '28',
        numToKeepStr: '5',
        artifactNumToKeepStr: '5'
      )
    )
  }

  triggers {
    pollSCM('H/30 * * * *')
  }

  stages {
    stage('Build NIDAS on all targets') {

      parallel {

        stage('CentOS8_x86_64') {
          agent {
            node {
              label 'CentOS8_x86_64'
            }
          }
          stages {
            stage('Compile and test') {
              steps {
                sh './jenkins.sh test'
              }
            }

            stage('Build RPM packages') {
              steps {
                sh './jenkins.sh snapshot'
              }
            }

            stage('Sign and Push RPM packages to EOL repository') {
              steps {
                sh './jenkins.sh push_rpms'
              }
            }

            stage('Update packages on local host') {
              when {
                environment name: 'NIDAS_UPDATE_HOST', value: 'true'
              }
              steps {
                sh './jenkins.sh update_rpms'
              }
            }
          }
        }

        stage('CentOS9_x86_64') {
          agent {
            node {
              label 'CentOS9_x86_64'
            }
          }
          stages {
            stage('Compile and test') {
              steps {
                sh './jenkins.sh test'
              }
            }

            stage('Build RPM packages') {
              steps {
                sh './jenkins.sh snapshot'
              }
            }

            stage('Sign and Push RPM packages to EOL repository') {
              steps {
                sh './jenkins.sh push_rpms'
              }
            }

            stage('Update packages on local host') {
              when {
                environment name: 'NIDAS_UPDATE_HOST', value: 'true'
              }
              steps {
                sh './jenkins.sh update_rpms'
              }
            }
          }
        }

        stage('Ubuntu32 (Vortex)') {
          agent {
            node {
              label 'CentOS8'
            }
          }

          stages {

//            stage('Compile and test') {
//              steps {
//                sh './jenkins.sh test'
//              }
//            }

            stage('Build Debian packages') {
              steps {
//                sh 'scripts/build_dpkg.sh -I bionic i386'
                sh 'scripts/run_podman.sh bionic /root/nidas/scripts/build_dpkg.sh -I bionic i386'
              }
            }
          }
        }

        stage('Fedora') {
          agent {
            node {
              label 'fedora'
            }
          }
          stages {
            stage('Compile and test') {
              steps {
                sh './jenkins.sh test'
              }
            }

            stage('Build RPM packages') {
              steps {
                sh './jenkins.sh snapshot'
              }
            }

            // skip signing and pushing rpms to EOL repo for now
          }
        }

      }
    }
  }

  post {
    changed
    {
      emailext from: "granger@ucar.edu",
        to: "granger@ucar.edu, cjw@ucar.edu",
        recipientProviders: [developers(), requestor()],
        subject: "Jenkins build ${env.JOB_NAME}: ${currentBuild.currentResult}",
        body: "Job ${env.JOB_NAME}: ${currentBuild.currentResult}\n${env.BUILD_URL}"
    }
  }

}
