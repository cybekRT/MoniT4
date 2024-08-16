if [[ $# -lt 1 ]]; then
	echo "Usage: $0 ip_address {delay}"
	exit 1
fi

IP=$1
NAME=`hostname`
DELAY=3

if [[ $# -ge 2 ]]; then
	DELAY=$2
fi

echo "Delay: $DELAY"

while [[ true ]]; do
	UPTIME=`cat /proc/uptime | cut -d" " -f1`
	CPU=`top -bn1 | grep "Cpu(s)" | sed "s/.*, *\([0-9.]*\)%* id.*/\1/" | awk '{print 100 - $1}'`
	RAM=`free -m | awk 'NR==2{printf "%.2f", $3*100/$2 }'`
	SWAP=`free -m | awk 'NR==3{printf "%.2f", $3*100/$2 }'`
	JSON="{\"name\": \"${NAME}\", \"uptime\": ${UPTIME}, \"cpu\": ${CPU}, \"ram\": ${RAM}, \"swap\": ${SWAP}}"
	echo $JSON

	sleep $DELAY
done | nc $IP 8000
