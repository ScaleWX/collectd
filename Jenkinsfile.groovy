def collectd=''
def filedata=''
def ssh=''
def rrdtool=''
def xml_definition=''
pipeline {
    agent any
    environment {
        COLLECTD_DIR="collectd"
        COLLECTD_REPO="git@github.com:ScaleWX/collectd.git"
        XML_DEFINITION_DIR="xml_definition"
        XML_DEFINITION_REPO="git@github.com:ScaleWX/xml_definition.git"
        CREDENTIALS_ID="<ID>"
        WORK_DIR="/var/lib/jenkins/work"
    }
    stages {
        stage('Prepare') {
            steps {
                dir(WORK_DIR) {
                    sh './cleanup.sh'
                }
            }
        }
        stage('Build') {
            steps {
                dir(COLLECTD_DIR) {
                    checkout([$class: 'GitSCM', branches: [[name: 'refs/tags/*']], extensions: [], userRemoteConfigs: [[credentialsId: CREDENTIALS_ID, url: COLLECTD_REPO]]])
                    sh './build.sh && ./configure && make rpms'
                }
                dir(XML_DEFINITION_DIR) {
                    checkout([$class: 'GitSCM', branches: [[name: 'refs/tags/*']], extensions: [], userRemoteConfigs: [[credentialsId: CREDENTIALS_ID, url: XML_DEFINITION_REPO]]])
                    sh './bootstrap.sh && ./configure && make rpm'
                }
            }
        }
        stage('Deploy') {
            steps {
                script {
                    collectd=sh(
                        script: 'basename `find . -type f -regextype posix-egrep -regex ".+/collectd-[0-9].+rpm" -print`',
                        returnStdout: true
                    ).trim()
                    filedata=sh(
                        script: 'basename `find . -type f -regextype posix-egrep -regex ".+/collectd-filedata.+rpm" -print`',
                        returnStdout: true
                    ).trim()
                    ssh=sh(
                        script: 'basename `find . -type f -regextype posix-egrep -regex ".+/collectd-ssh.+rpm" -print`',
                        returnStdout: true
                    ).trim()
                    rrdtool=sh(
                        script: 'basename `find . -type f -regextype posix-egrep -regex ".+/collectd-rrdtool.+rpm" -print`',
                        returnStdout: true
                    ).trim()
                }
                dir(COLLECTD_DIR) {
                    sh "sudo rpm -Uvh $collectd $filedata $ssh $rrdtool"
                }
                script {
                    xml_definition=sh(
                        script: 'basename `find . -type f -regextype posix-egrep -regex ".+/xml_definition.+noarch.+rpm" -print`',
                        returnStdout: true
                    ).trim()
                }
                dir(XML_DEFINITION_DIR) {
                    sh "sudo rpm -Uvh RPMS/noarch/$xml_definition"
                }
            }
        }
        stage('Test') {
            steps {
                dir(WORK_DIR) {
                    sh './initdb.sh'
                }
                dir(WORK_DIR) {
                    sh 'python verify_metrics.py -d /var/lib/jenkins/work -f /etc/lustre-b_es5_2.xml -t tests.xml -c ./collectd.conf -w yes'
                }
                dir(WORK_DIR) {
                    sh './check_tsdb_test_results.sh'
                }
            }
        }
        stage('Cleanup') {
            steps {
                dir(WORK_DIR) {
                    sh './cleanup.sh'
                }
            }
        }
        stage('Release') {
            environment {
                GITHUB_TOKEN='<TOKEN>'
            }
            steps {
                dir(COLLECTD_DIR) {
                    sh './publish.sh $GITHUB_TOKEN LustrePerfMon-5.12.0.148 ScaleWX/collectd'
                }
            }
        }
    }
}
