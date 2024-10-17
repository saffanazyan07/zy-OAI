#!/bin/bash
rm -rf logs
mkdir logs
grep container_name docker-compose.yaml  | grep -v "#" | sed s/container_name:// | sed s/\"//g | while read -r line ; do
   docker logs $line > logs/$line.log 2>&1
done
#Try but don't fail
cp /var/log/aerial/phy.log logs || :
cp /var/log/aerial/nvipc.pcap logs || :
cp cn.pcap logs || :

tar -cvzf l2plusLogs-$(date +%F-%H%M%S).tgz logs
