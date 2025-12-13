#!/bin/sh
#
# consh_nw_setup.sh
#
# (C) 2025, Michael Kerrisk
#
# Licensed under the GNU General Public License version 2 or later

if [ $# -ne 4 ] ; then
cat << EOF 1>&2
Usage: $0 <consh-PID> <netns-name> <IP-addr-0> <IP-addr-1>"

where:
    consh-PID	is the PID of the container shell
    netns-name	is the name to be used for the bind mount needed by 'ip netns'
    IP-addr-0	is the network ID to be assigned to the veth end point that
		sits outside the container namespace
    IP-addr-1	is the network ID to be assigned to the veth end point that
		sits inside the container namespace

Example:
    $0 1234 consh 10.0.0.1/24 10.0.0.2/24
EOF
    exit 1
fi

consh_pid=$1
netns_name=$2
ipaddr_0=$3
ipaddr_1=$4

# Create the bind mount that is needed by 'ip netns'

sudo mkdir -p /var/run/netns		# Ensure directory exists
linkpath=/var/run/netns/$netns_name
sudo touch $linkpath
if [ $? -ne 0 ] ; then
    echo "Failed to create bind mount file ($linkpath)" 1>&2
    exit 1
fi

sudo mount --bind /proc/$consh_pid/ns/net $linkpath

# Create a veth pair

vethname="$netns_name-$$"
veth_0="$vethname-0"
veth_1="$vethname-1"

sudo ip link add $veth_0 type veth peer name $veth_1

# Move one end of the veth pair into the container network namespace

sudo ip link set $veth_1 netns $netns_name

# Assign addresses to the veth end points and bring them up

sudo ip addr add $ipaddr_0 dev $veth_0
sudo ip link set $veth_0 up

sudo ip netns exec $netns_name ip address add $ipaddr_1 dev $veth_1
sudo ip netns exec $netns_name ip link set $veth_1 up
