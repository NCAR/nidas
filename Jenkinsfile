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

//   triggers {
//     pollSCM('H/30 * * * *')
//   }

  stages {
    stage('Build NIDAS on all targets') {
      parallel {
        stage('RPi Bookworm') {
          agent { dockerfile true }
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
      } // parallel stages
    } // stage('Build NIDAS on all targets')

//   post {
//     changed
//     {
//       emailext from: "granger@ucar.edu",
//         to: "granger@ucar.edu, cjw@ucar.edu, cdewerd@ucar.edu",
//         recipientProviders: [developers(), requestor()],
//         subject: "Jenkins build ${env.JOB_NAME}: ${currentBuild.currentResult}",
//         body: "Job ${env.JOB_NAME}: ${currentBuild.currentResult}\n${env.BUILD_URL}"
//     }
  } // sages
} // pipeline
