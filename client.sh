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
	if [[ x"$JSON_INIT_STORAGE" != x"" ]]; then
		JSON_INIT_STORAGE="$JSON_INIT_STORAGE, "
	fi
	JSON_INIT_STORAGE="$JSON_INIT_STORAGE\"$STORAGE\""
done
JSON_INIT_STORAGE="[$JSON_INIT_STORAGE]"

#echo $JSON_INIT_STORAGE
#exit

JSON_INIT="{\"init\": { \"name\": $NAME, \"usage\": [\"cpu\", \"ram\", \"swp\"], \"storage\": $JSON_INIT_STORAGE } }"

# Print free storage
df -h -t ext4 | tail -n+2 | tr -s " " | cut -d" " -f5,6 | while read LINE; do
	NAME=`echo $LINE | cut -d" " -f2`
	USED=`echo $LINE | cut -d" " -f1`
	USED=${USED::-1}
	echo "$NAME = $USED"
done

(echo $JSON_INIT &&
while [[ true ]]; do
	#UPTIME=`cat /proc/uptime | cut -d" " -f1`
	CPU=`top -bn1 | grep "Cpu(s)" | sed "s/.*, *\([0-9.]*\)%* id.*/\1/" | awk '{print 100 - $1}'`
	RAM=`free -m | awk 'NR==2{printf "%.2f", $3*100/$2 }'`
	SWAP=`free -m | awk 'NR==3{printf "%.2f", $3*100/$2 }'`

	JSON_USAGE="{\"cpu\": $CPU, \"ram\": $RAM, \"swp\": $SWAP}"

	#JSON="{\"name\": \"${NAME}\", \"uptime\": ${UPTIME}, \"cpu\": ${CPU}, \"ram\": ${RAM}, \"swap\": ${SWAP}}"
	JSON="{\"usage\": $JSON_USAGE}"
	echo $JSON

	sleep $DELAY
done) | nc $IP $PORT
