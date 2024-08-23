#!/bin/bash

#set -x
if [[ $# -lt 1 ]]; then
	echo "Usage: $0 ip_address {delay} {port}"
	exit 1
fi

IP=$1
NAME=`hostname`
DELAY=3
PORT=8000

if [[ $# -ge 2 ]]; then
	DELAY=$2
fi

if [[ $# -ge 3 ]]; then
	PORT=$3
fi

#echo "Delay: $DELAY"

JSON_INIT_STORAGE=""
STORAGE_LIST=`df -h -t ext4 | tr -s " " | cut -d" " -f6 | tail -n +2`
for STORAGE in $STORAGE_LIST; do
	if [[ "$JSON_INIT_STORAGE" != "" ]]; then
		JSON_INIT_STORAGE="$JSON_INIT_STORAGE, "
	fi
	JSON_INIT_STORAGE="$JSON_INIT_STORAGE\"$STORAGE\""
done
JSON_INIT_STORAGE="[$JSON_INIT_STORAGE]"

#echo $JSON_INIT_STORAGE
#exit

JSON_INIT="{\"init\": { \"name\": \"$NAME\", \"temperature\": [\"cpu\"], \"usage\": [\"cpu\", \"ram\", \"swp\"], \"storage\": $JSON_INIT_STORAGE, \"network\": [\"192.168.100.1\", \"10.0.2.3\", \"8.8.8.8\", \"bing.com\"] } }"

# Print free storage
function GetStorage()
{
	JSON_STORAGE=""
	#df -h -t ext4 | tail -n+2 | tr -s " " | cut -d" " -f5,6 | while read LINE; do
	while IFS= read -r LINE; do
		if [[ "$JSON_STORAGE" != "" ]]; then
			JSON_STORAGE="$JSON_STORAGE, "
		fi
		NAME=`echo $LINE | cut -d" " -f2`
		USED=`echo $LINE | cut -d" " -f1`
		USED=${USED::-1}

		JSON_STORAGE="$JSON_STORAGE\"$NAME\": $USED"
		#echo $JSON_STORAGE
		#echo "$NAME = $USED"
	done <<< `df -h -t ext4 | tail -n+2 | tr -s " " | cut -d" " -f5,6`

	echo "{\"storage\": {$JSON_STORAGE}}"
}

#YOLO=`GetStorage`
#echo "Yolo: $YOLO"

#JSON_STORAGE="{$JSON_STORAGE}"
#echo $JSON_STORAGE

(echo $JSON_INIT && GetStorage &&
while [[ true ]]; do
	#UPTIME=`cat /proc/uptime | cut -d" " -f1`
	CPU=`top -bn1 | grep "Cpu(s)" | sed "s/.*, *\([0-9.]*\)%* id.*/\1/" | awk '{print 100 - $1}'`
	RAM=`free -m | awk 'NR==2{printf "%.2f", $3*100/$2 }'`
	SWAP=`free -m | awk 'NR==3{printf "%.2f", $3*100/$2 }'`

	JSON_USAGE="{\"cpu\": $CPU, \"ram\": $RAM, \"swp\": $SWAP}"

	#JSON="{\"name\": \"${NAME}\", \"uptime\": ${UPTIME}, \"cpu\": ${CPU}, \"ram\": ${RAM}, \"swap\": ${SWAP}}"
	JSON="{\"usage\": $JSON_USAGE}"
	echo $JSON

	NET_8888=`ping -W1 -c1 8.8.8.8 | head -n2 | tail -n1 | cut -d= -f4 | cut -d" " -f1`
	NET_ROUTER=`ping -W1 -c1 192.168.100.1 | head -n2 | tail -n1 | cut -d= -f4 | cut -d" " -f1`
	NET_WG=`ping -W1 -c1 10.0.2.3 | head -n2 | tail -n1 | cut -d= -f4 | cut -d" " -f1`
	NET_BING=`ping -W1 -c1 bing.com | head -n2 | tail -n1 | cut -d= -f4 | cut -d" " -f1`
	#NET_BING=`ping -W1 -c1 bing.com | head -n2 | tail -n1 | cut -d" " -f8 | cut -d= -f2`

	if [[ "$NET_8888" == "" ]]; then
		NET_8888=-1
	fi
	if [[ "$NET_ROUTER" == "" ]]; then
		NET_ROUTER=-1
	fi
	if [[ "$NET_WG" == "" ]]; then
		NET_WG=-1
	fi
	if [[ "$NET_BING" == "" ]]; then
		NET_BING=-1
	fi

	TEMP_CPU=`cat /sys/devices/pci0000\:00/0000\:00\:18.3/hwmon/hwmon2/temp2_input`
	TEMP_CPU=$((TEMP_CPU/1000))

	JSON_NET="{\"temperature\": {\"cpu\": $TEMP_CPU}, \"network\": {\"8.8.8.8\": $NET_8888, \"192.168.100.1\": $NET_ROUTER, \"10.0.2.3\": $NET_WG, \"bing.com\": $NET_BING}}"
	echo $JSON_NET

	sleep $DELAY
done) | nc $IP $PORT
