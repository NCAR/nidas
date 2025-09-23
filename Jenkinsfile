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
                    args '-v $WORKSPACE:/workspase -w /workspace -u root'
                    // reuseNode true
                }
            }
            stages {
                stage('Compile and test') {
                    steps {
                        // sh 'bash -c "pwd"'
                        // sh 'bash -c "ls -la"'
                        // sh 'bash -lc "echo CPP: ${CXX} ${PKG_CONFIG}" '
                        // sh 'bash -lc "cat ~/.bashrc" '
                        sh 'bash -lc "pushd src && scons -j$(nproc) --no-cache && popd"'
                        // sh 'bash -c "./jenkins.sh test"'
                    }
                }
                stage('Build packages') {
                    steps {
                        sh 'bash -lc "pushd scripts && ./build_dpkg.sh -I bookworm arm64 && popd"'
                    }
                }
                stage('Sign and Push packages to EOL repository') {
                    steps {
                        sh 'echo "ADD STEP TO PUSH RPi Bookworm packages to EOL Repo"'
                    }
                }
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
