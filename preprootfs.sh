cd bin
mkdir -p rootfs
mkdir -p rootfs/etc
cp /etc/hosts rootfs/etc/hosts
echo "nameserver 1.1.1.1" > rootfs/etc/resolv.conf
cp /etc/nsswitch.conf rootfs/etc/nsswitch.conf
mkdir -p ./rootfs/etc/ssl/certs
mkdir -p ./rootfs/etc/pki/tls/certs
if [ -d "/etc/ssl/certs" ]; then
    cp -RL /etc/ssl/certs/* ./rootfs/etc/ssl/certs/
fi
if [ -d "/etc/pki/tls/certs" ]; then
    cp -RL /etc/pki/tls/certs/* ./rootfs/etc/pki/tls/certs/
fi