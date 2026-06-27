pipeline {

  // This pipeline uses the NIDAS_UPDATE_HOST environment variable to
  // determine whether to update the local host with the newly built packages.
  // Set NIDAS_UPDATE_HOST to true in the Jenkins Node configuration to enable
  // this step for a specific node.
  //
  // Manage Jenkins -> Nodes -> <node name> -> Configure -> Node Properties
  // 
  // Add NIDAS_UPDATE_HOST environment variable with value true.

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
            stage('Update submodules') {
              steps {
                sh 'git submodule update --init --recursive'
              }
            }
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
            stage('Update submodules') {
              steps {
                sh 'git submodule update --init --recursive'
              }
            }
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
              label 'CentOS9_x86_64'
            }
          }

          stages {

            stage('Update submodules') {
              steps {
                sh 'git submodule update --init --recursive'
              }
            }

            stage('Build Debian packages') {
              steps {
                sh './jenkins.sh build_bionic'
              }
            }

            stage('Upload Debian packages to EOL repository') {
              steps {
                sh './jenkins.sh upload_bionic'
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

            stage('Update submodules') {
              steps {
                sh 'git submodule update --init --recursive'
              }
            }

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
        to: "granger@ucar.edu, cjw@ucar.edu, cdewerd@ucar.edu",
        recipientProviders: [developers(), requestor()],
        subject: "Jenkins build ${env.JOB_NAME}: ${currentBuild.currentResult}",
        body: "Job ${env.JOB_NAME}: ${currentBuild.currentResult}\n${env.BUILD_URL}"
    }
  }

}
