/* groovylint-disable NestedBlockDepth */
pipeline {
  environment {
    CONTAINER_NODE = 'CentOS8'
  }
  // Everything runs on the container node, especially the containers.
  agent {
    node {
      label "${CONTAINER_NODE}"
    }
  }

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
                  label "${CONTAINER_NODE}"
                  dir 'scripts/docker'
                  filename 'Dockerfile.debian_cross_arm64'
                  args '-v $WORKSPACE:/workspace -w /workspace -u root'
                  additionalBuildArgs '--build-arg HOST_ARCH=arm64 --build-arg CODENAME=bookworm'
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
            stage('Upload Bookworm packages to repo') {
              steps {
                sh './jenkins.sh upload_dsm3_debs codename=bookworm'
              }
            }
          } // stages
        } // stage('Bookworm')

        stage('Trixie') {
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
                  label "${CONTAINER_NODE}"
                  dir 'scripts/docker'
                  filename 'Dockerfile.debian_cross_arm64'
                  args '-v $WORKSPACE:/workspace -w /workspace -u root'
                  additionalBuildArgs '--build-arg HOST_ARCH=arm64 --build-arg CODENAME=trixie'
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
} // pipeline
