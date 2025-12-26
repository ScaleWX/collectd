pipeline {
    agent none
    options {
        skipDefaultCheckout(true)
    }
    environment {
        COLLECTD_DIR="collectd"
        COLLECTD_REPO="git@github.com:ScaleWX/collectd.git"
        XML_DEFINITION_DIR="xml_definition"
        XML_DEFINITION_REPO="git@github.com:ScaleWX/xml_definition.git"
        CREDENTIALS_ID="jenkins-private-key"
        GITHUB_TOKEN=credentials('github-token')
        WORK_DIR="/var/lib/jenkins/work"
    }
    stages {
        stage('Build Matrix') {
            matrix {
                axes {
                    axis {
                        name 'OS'
                        values 'el8', 'el9', 'el10'
                    }
                }
                agent { label "${OS}" }
                stages {
                    stage('Build') {
                        steps {
                            script {
                                dir(COLLECTD_DIR) {
                                    deleteDir()
                                    checkout([$class: 'GitSCM', branches: [[name: '*/LustrePerfMon-5.12.0.148']], extensions: [[$class: 'CloneOption', fetchTags: true]], userRemoteConfigs: [[credentialsId: CREDENTIALS_ID, url: COLLECTD_REPO]]])
                                    sh "./autobuild.sh $OS"
                                }
                                dir(XML_DEFINITION_DIR) {
                                    deleteDir()
                                    checkout([$class: 'GitSCM', branches: [[name: '*/main']], extensions: [[$class: 'CloneOption', fetchTags: true]], userRemoteConfigs: [[credentialsId: CREDENTIALS_ID, url: XML_DEFINITION_REPO]]])
                                    sh "./bootstrap.sh && ./configure && make rpm"
                                }
                            }
                        }
                    }
                    stage('Deploy') {
                        steps {
                            script {
                                dir(COLLECTD_DIR) {
                                    sh "./cleanup.sh"
                                    def collectd = sh(
                                        script: 'basename `find . -type f -regextype posix-egrep -regex ".+/collectd-[0-9].+rpm" -print`',
                                        returnStdout: true
                                    ).trim()
                                    def filedata = sh(
                                        script: 'basename `find . -type f -regextype posix-egrep -regex ".+/collectd-filedata-[0-9].+rpm" -print`',
                                        returnStdout: true
                                    ).trim()
                                    def ssh = sh(
                                        script: 'basename `find . -type f -regextype posix-egrep -regex ".+/collectd-ssh-[0-9].+rpm" -print`',
                                        returnStdout: true
                                    ).trim()
                                    def rrdtool = sh(
                                        script: 'basename `find . -type f -regextype posix-egrep -regex ".+/collectd-rrdtool-[0-9].+rpm" -print`',
                                        returnStdout: true
                                    ).trim()
                                    sh "sudo rpm -Uvh $collectd $filedata $ssh $rrdtool"
                                }
                                dir(XML_DEFINITION_DIR) {
                                    def xml_definition = sh(
                                        script: 'basename `find . -type f -regextype posix-egrep -regex ".+/filedata_definition.+noarch.+rpm" -print`',
                                        returnStdout: true
                                    ).trim()
                                    sh "sudo rpm -Uvh RPMS/noarch/$xml_definition"
                                }
                            }
                        }
                    }
                    stage('Test') {
                        steps {
                            dir(COLLECTD_DIR) {
                                lock(resource: 'monitor01') {
                                    sh "./initdb.sh"
                                    sh "python3 verify_metrics.py -d ${WORK_DIR} -f /etc/filedata/lustre-2.12.9_ddn27.xml -t ${WORK_DIR}/tests.xml -c ${WORK_DIR}/collectd.conf -w yes -i 30"
                                    sh "./check_tsdb_test_results.sh"
                                }
                            }
                        }
                    }
                    stage('Cleanup') {
                        steps {
                            sh "sudo rpm -e collectd collectd-ssh collectd-rrdtool collectd-filedata filedata_definition"
                        }
                    }
                }
            }
        }
        stage('Release') {
            when {
                expression {
                    node('el8') {
                        dir(COLLECTD_DIR) {
                            def status = sh(script: "git describe --tags --exact-match HEAD", returnStatus: true)
                            return status == 0
                        }
                    }
                }
            }
            stages {
                stage('Create Release') {
                    agent { label 'el8' }
                    steps {
                        dir(COLLECTD_DIR) {
                            sh "./create_release $GITHUB_TOKEN LustrePerfMon-5.12.0.148 ScaleWX/collectd"
                        }
                    }
                }
                stage('Upload Matrix') {
                    matrix {
                        axes {
                            axis {
                                name 'OS'
                                values 'el8', 'el9', 'el10', 'ubuntu'
                            }
                        }
                        agent { label "${OS}" }
                        stages {
                            stage('Upload Packages') {
                                steps {
                                    script {
                                        dir(COLLECTD_DIR) {
                                            if (OS == 'ubuntu') {
                                                deleteDir()
                                                checkout([$class: 'GitSCM', branches: [[name: '*/LustrePerfMon-5.12.0.148']], extensions: [[$class: 'CloneOption', fetchTags: true]], userRemoteConfigs: [[credentialsId: CREDENTIALS_ID, url: COLLECTD_REPO]]])
                                                sh "./autobuild.sh $OS"
                                            }
                                            sh "./upload_artifacts.sh $GITHUB_TOKEN ScaleWX/collectd $OS"
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
