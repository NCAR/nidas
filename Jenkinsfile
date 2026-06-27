/* groovylint-disable NestedBlockDepth */

// Run this pipeline and the containers it contains on mercury, where the
// jenkins home directory (and thus default workspace and default podman
// storage) are on a large local disk.  Each container build specifies a
// custom workspace so the build artifacts, especially package files, cannot
// interfere with each other.

def CONTAINER_LABEL = 'CentOS8'

pipeline {

  // it is helpful to not have a top-level agent, otherwise it is allocated to
  // an executor for the entire duration of the pipeline, preventing the
  // parallel stages from running in parallel if there are only two executors
  // on the node.
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

  stages {
    stage('Build NIDAS for Raspberry Pi') {
      parallel {
        stage('Bookworm') {
          agent {
            node {
              label CONTAINER_LABEL
              customWorkspace "${env.JOB_BASE_NAME}-bookworm"
            }
          }
          stages {
            stage('Checkout NCAR Nidas') {
              steps {
                // Clone NCAR/nidas repository, branch 'buster'
                git url: 'https://github.com/NCAR/nidas.git', branch: 'buster'
              }
            } // stage('Checkout NCAR Nidas')
            stage('Build in bookworm container') {
              agent {
                dockerfile {
                  reuseNode true
                  dir 'scripts/docker'
                  filename 'Dockerfile.debian_cross_arm64'
                  args '-v $WORKSPACE:/workspace -w /workspace -u root'
                  additionalBuildArgs '--build-arg HOST_ARCH=arm64 --build-arg CODENAME=bookworm'
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
            stage('Upload Bookworm packages to repo') {
              steps {
                sh './jenkins.sh upload_dsm3_debs codename=bookworm'
              }
            }
          } // stages
        } // stage('Bookworm')

        stage('Trixie') {
          agent {
            node {
              label CONTAINER_LABEL
              customWorkspace "${env.JOB_BASE_NAME}-trixie"
            }
          }
          stages {
            stage('Checkout NCAR Nidas') {
              steps {
                // Clone NCAR/nidas repository, branch 'buster'
                git url: 'https://github.com/NCAR/nidas.git', branch: 'buster'
              }
            } // stage('Checkout NCAR Nidas')
            stage('Build in trixie container') {
              agent {
                dockerfile {
                  reuseNode true
                  dir 'scripts/docker'
                  filename 'Dockerfile.debian_cross_arm64'
                  args '-v $WORKSPACE:/workspace -w /workspace -u root'
                  additionalBuildArgs '--build-arg HOST_ARCH=arm64 --build-arg CODENAME=trixie'
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
            } // stage('Build in trixie container')
            stage('Upload Trixie packages to repo') {
              steps {
                sh './jenkins.sh upload_dsm3_debs codename=trixie'
              }
            }
          } // stages
        } // stage('Trixie')
      } // parallel
    } // build for raspberry pi
  } // stages

  post {
    changed
    {
      emailext from: "granger@ucar.edu",
        to: "granger@ucar.edu",
        recipientProviders: [developers(), requestor()],
        subject: "Jenkins build ${env.JOB_NAME}: ${currentBuild.currentResult}",
        body: "Job ${env.JOB_NAME}: ${currentBuild.currentResult}\n${env.BUILD_URL}"
    }
  }

} // pipeline
