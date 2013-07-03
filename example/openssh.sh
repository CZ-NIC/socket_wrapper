#!/bin/bash

SSH_DIRECTORY=$(mktemp -d /tmp/tmp.swrap.XXXXXXXX)
SERVER_ADDRESS="127.0.0.10"
mkdir ${SSH_DIRECTORY}/swrap

cleanup_and_exit () {
    trap EXIT
    test -z "$1" && set 0

    echo
    echo "CLEANING UP"
    echo

    kill -TERM $(< ${SSH_DIRECTORY}/sshd.pid)
    rm -rf ${SSH_DIRECTORY}

    exit $1
}

# Setup exit handler
trap cleanup_and_exit SIGINT SIGTERM

echo Generating ${SSH_DIRECTORY}/ssh_host_key.
ssh-keygen -t rsa1 -b 2048 -f ${SSH_DIRECTORY}/ssh_host_key -N '' 2>/dev/null
echo Generating ${SSH_DIRECTORY}/ssh_host_dsa_key.
ssh-keygen -t dsa -f ${SSH_DIRECTORY}/ssh_host_dsa_key -N '' 2>/dev/null
echo Generating ${SSH_DIRECTORY}/ssh_host_rsa_key.
ssh-keygen -t rsa -b 2048 -f ${SSH_DIRECTORY}/ssh_host_rsa_key -N '' 2>/dev/null
#echo Generating ${SSH_DIRECTORY}/ssh_host_ecdsa_key.
#ssh-keygen -t ecdsa -b 256 -f ${SSH_DIRECTORY}/ssh_host_ecdsa_key -N '' 2>/dev/null

# Create sshd_config file
cat > ${SSH_DIRECTORY}/sshd_config << EOT
Port 22
ListenAddress ${SERVER_ADDRESS}
HostKey ${SSH_DIRECTORY}/ssh_host_key
HostKey ${SSH_DIRECTORY}/ssh_host_rsa_key
HostKey ${SSH_DIRECTORY}/ssh_host_dsa_key
#HostKey ${SSH_DIRECTORY}/ssh_host_ecdsa_key
Subsystem sftp /usr/lib/ssh/sftp-server

LogLevel DEBUG1

AcceptEnv LANG LC_CTYPE LC_NUMERIC LC_TIME LC_COLLATE LC_MONETARY LC_MESSAGES
AcceptEnv LC_PAPER LC_NAME LC_ADDRESS LC_TELEPHONE LC_MEASUREMENT
AcceptEnv LC_IDENTIFICATION LC_ALL

PidFile ${SSH_DIRECTORY}/sshd.pid
EOT

export SOCKET_WRAPPER_DIR="${SSH_DIRECTORY}/swrap"
export SOCKET_WRAPPER_DEFAULT_IFACE=11

echo
echo "Starting SSHD with SOCKET_WRAPPER_DIR=${SSH_DIRECTORY}/swrap ..."
DYLD_INSERT_LIBRARIES=libsocket_wrapper.dylib LD_PRELOAD=libsocket_wrapper.so /usr/sbin/sshd -f ${SSH_DIRECTORY}/sshd_config -e 2> ${SSH_DIRECTORY}/sshd_log || cleanup_and_exit 1
echo "done"

echo
echo "Connecting to the ${SERVER_ADDRESS} ssh server using ssh binary."
echo "You can check the sshd log file at ${SSH_DIRECTORY}/sshd_log."
echo "If you logout sshd will be stopped and the environment cleaned up."
DYLD_INSERT_LIBRARIES=libsocket_wrapper.dylib LD_PRELOAD=libsocket_wrapper.so ssh -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no ${SERVER_ADDRESS}

cleanup_and_exit 0
