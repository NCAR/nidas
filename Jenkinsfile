/* groovylint-disable NestedBlockDepth */
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
        stage('Checkout NCAR Nidas') {
            agent {
                node {
                    label 'CentOS9'
                }
            }
            steps {
                // Clone NCAR/nidas repository, branch 'buster'
                git url: 'https://github.com/NCAR/nidas.git', branch: 'buster-SKF'
            }
        } // stage('Checkout NCAR Nidas')
        stage('RPi Bookworm') {
            agent {
                dockerfile {
                    filename 'Dockerfile'
                    label 'CentOS9'
                    dir '.'
                    args '-v $WORKSPACE:/workspace -w /workspace -u root'
                    // reuseNode true
                }
            }
            stages {
                stage('Compile') {
                    steps {
                        sh './jenkins.sh compile BUILDS=arm64'
                    }
                }
                stage('Build packages') {
                    steps {
                        sh './jenkins.sh build_dsm3_debs'
                    }
                }
            }
        }
        stage('Upload packages to repo') {
            agent {
                node {
                    label 'CentOS9'
                }
            }
            steps {
              sh './jenkins.sh upload_dsm3_debs'
            }
        }

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
