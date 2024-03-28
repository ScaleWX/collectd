def collectd=''
def filedata=''
def ssh=''
def rrdtool=''
def xml_definition=''
pipeline {
    agent none
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
            agent { label 'el7' }
            steps {
                dir(WORK_DIR) {
                    sh './cleanup.sh'
                }
            }
        }
        stage('Build') {
            agent { label 'el7' }
            steps {
                dir(COLLECTD_DIR) {
		    sh 'rm -rf *'
                    checkout([$class: 'GitSCM', branches: [[name: 'refs/tags/*']], extensions: [], userRemoteConfigs: [[credentialsId: CREDENTIALS_ID, url: COLLECTD_REPO]]])
                    sh './build.sh && ./configure && make rpms'
                }
                dir(XML_DEFINITION_DIR) {
		    sh 'rm -rf *'
                    checkout([$class: 'GitSCM', branches: [[name: 'refs/tags/*']], extensions: [], userRemoteConfigs: [[credentialsId: CREDENTIALS_ID, url: XML_DEFINITION_REPO]]])
                    sh './bootstrap.sh && ./configure && make rpm'
                }
            }
        }
        stage('Deploy') {
            agent { label 'el7' }
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
                        script: 'basename `find . -type f -regextype posix-egrep -regex ".+/filedata_definition.+noarch.+rpm" -print`',
                        returnStdout: true
                    ).trim()
                }
                dir(XML_DEFINITION_DIR) {
                    sh "sudo rpm -Uvh RPMS/noarch/$xml_definition"
                }
            }
        }
        stage('Test') {
            agent { label 'el7' }
            steps {
                dir(WORK_DIR) {
                    sh './initdb.sh'
                }
                dir(WORK_DIR) {
                    sh 'python verify_metrics.py -d /var/lib/jenkins/work -f /etc/filedata/lustre-2.12.9_ddn27.xml -t tests.xml -c ./collectd.conf -w yes -i 30'
                }
                dir(WORK_DIR) {
                    sh './check_tsdb_test_results.sh'
                }
            }
        }
        stage('Cleanup') {
            agent { label 'el7' }
            steps {
                sh 'sudo rpm -e collectd collectd-ssh collectd-rrdtool collectd-filedata filedata_definition'
            }
        }
        stage('Release') {
            agent { label 'el7' }
            environment {
            GITHUB_TOKEN='<TOKEN>'
            }
            steps {
                dir(COLLECTD_DIR) {
                    sh './create_release $GITHUB_TOKEN LustrePerfMon-5.12.0.148 ScaleWX/collectd && ./upload_artifacts $GITHUB_TOKEN ScaleWX/collectd el7'
                }
            }
        }
        stage('Upload el8 Packages') {
            agent { label 'el8' }
            environment {
                GITHUB_TOKEN='<TOKEN>'
            }
            steps {
                dir(COLLECTD_DIR) {
		    sh 'rm -rf *'
                    checkout([$class: 'GitSCM', branches: [[name: 'refs/tags/*']], extensions: [], userRemoteConfigs: [[credentialsId: CREDENTIALS_ID, url: COLLECTD_REPO]]])
                    sh './upload_artifacts $GITHUB_TOKEN ScaleWX/collectd el8'
                }
            }
        }
        stage('Upload el9 Packages') {
            agent { label 'el9' }
            environment {
                GITHUB_TOKEN='<TOKEN>'
            }
            steps {
                dir(COLLECTD_DIR) {
		    sh 'rm -rf *'
                    checkout([$class: 'GitSCM', branches: [[name: 'refs/tags/*']], extensions: [], userRemoteConfigs: [[credentialsId: CREDENTIALS_ID, url: COLLECTD_REPO]]])
                    sh './upload_artifacts $GITHUB_TOKEN ScaleWX/collectd el9'
                }
            }
        }
	stage('Upload Ubuntu Packages') {
            agent { label 'ubuntu' }
            environment {
                GITHUB_TOKEN='<TOKEN>'
            }
            steps {
                dir(COLLECTD_DIR) {
		    sh 'rm -rf *'
                    checkout([$class: 'GitSCM', branches: [[name: 'refs/tags/*']], extensions: [], userRemoteConfigs: [[credentialsId: CREDENTIALS_ID, url: COLLECTD_REPO]]])
                    sh './upload_artifacts $GITHUB_TOKEN ScaleWX/collectd ubuntu'
                }
            }
        }
    }
}
