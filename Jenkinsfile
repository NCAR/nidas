/* groovylint-disable NestedBlockDepth */
pipeline {
  agent {
    // only mercury has enough local disk space to build the containers, so
    // this hopefully forces the entire pipeline to run on mercury
    label 'CentOS8'
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
              agent {
                node {
                  label 'CentOS9'
                }
              }
              steps {
                // Clone NCAR/nidas repository, branch 'buster'
                git url: 'https://github.com/NCAR/nidas.git', branch: 'buster'
              }
            } // stage('Checkout NCAR Nidas')
            stage('Build in bookworm container') {
              agent {
                dockerfile {
                  filename 'scripts/docker/Dockerfile.debian_cross_arm64'
                  label 'CentOS9'
                  dir '.'
                  additionalBuildArgs '--build-arg HOST_ARCH=arm64 --build-arg CODENAME=bookworm'
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
                sh './jenkins.sh upload_dsm3_debs codename=bookworm'
              }
            }
          } // stages
        } // stage('Bookworm')

        stage('Trixie') {
          stages {
            stage('Checkout NCAR Nidas') {
              agent {
                node {
                  label 'CentOS9'
                }
              }
              steps {
                // Clone NCAR/nidas repository, branch 'buster'
                git url: 'https://github.com/NCAR/nidas.git', branch: 'buster'
              }
            } // stage('Checkout NCAR Nidas')
            stage('Build in trixie container') {
              agent {
                dockerfile {
                  filename 'scripts/docker/Dockerfile.debian_cross_arm64'
                  label 'CentOS9'
                  dir '.'
                  additionalBuildArgs '--build-arg HOST_ARCH=arm64 --build-arg CODENAME=trixie'
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
            } // stage('Build in trixie container')
            stage('Upload packages to repo') {
              agent {
                node {
                  label 'CentOS9'
                }
              }
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
