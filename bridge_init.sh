#!/bin/bash

# your bridge name here
BRIDGE_NAME="br0"
# your bridge ip here
NEW_IP="192.168.1.2/16"


# Check if br0 bridge exists
if ! brctl show | grep -q "$BRIDGE_NAME"; then
    # Create br0 bridge if it doesn't exist
    echo "Creating $BRIDGE_NAME bridge..."
    brctl addbr $BRIDGE_NAME
fi

# Check if br0 bridge has an IP address
if ip addr show $BRIDGE_NAME | grep -q "inet "; then
    # Delete the existing IP address on br0 bridge
    echo "Deleting existing IP address on $BRIDGE_NAME bridge..."
    ip addr del $(ip addr show $BRIDGE_NAME | grep "inet " | awk '{print $2}') dev $BRIDGE_NAME
fi

# Add the new IP address to br0 bridge
echo "Adding new IP address $NEW_IP to $BRIDGE_NAME bridge..."
ip addr add $NEW_IP dev $BRIDGE_NAME

echo "Getting iptables MASQUERADE nat ips..."
EXISTING_IPS=$(sudo iptables -t nat -L | grep MASQUERADE | awk '{print $4}')


echo "Configuration firewall."
if [[ ! "$EXISTING_IPS" =~ "$NEW_IP" ]]; then
    sudo iptables -t nat -A POSTROUTING -s $NEW_IP ! -o $BRIDGE_NAME -j MASQUERADE
    echo "New iptables nat rule added for $NEW_IP"
else
    echo "iptables nat rule already exists for $NEW_IP"
fi

echo "Activate bridge up"
sudo ip link set dev $BRIDGE_NAME up

echo "Activate ip forwarding function"
sysctl net.ipv4.conf.all.forwarding=1

echo "Configuration completed."

# sudo sysctl -w net.ipv4.ip_forward=1

