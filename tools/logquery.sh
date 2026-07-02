#!/bin/bash
#
# logquery.sh - query the agentsh audit trail by agent and month.
#
# The agentsh audit logs use the line format
#
#   Mon/DD/YYYY HH:MM:SS <agent> <command> <ALLOW|DENY> <args...>
#
# so the same first-line date detection + name grep works unchanged.
#
# Usage:   ./logquery.sh <audit dir> <agent> <month>
# Example: ./logquery.sh audit alice Jul
#
# Produces queryResults/<agent>_<Mon>_<yyyy>.log, one file per matching year.

if [ "$#" -ne 3 ]; then
	echo "Usage: ./logquery.sh <audit dir> <agent> <month>"
	exit 1
fi

AUDIT_DIR=$1
AGENT=$2

case "$3" in
	"Jan"|"Feb"|"Mar"|"Apr"|"May"|"Jun"|"Jul"|"Aug"|"Sep"|"Oct"|"Nov"|"Dec")
		MONTH=$3
		;;
	*)
		echo "Invalid month"
		exit 1
		;;
esac

mkdir -p queryResults
rm -rf queryResults/*.log

LOG_FILES=$(find "$AUDIT_DIR" -type f)
DATE_REGEX="$MONTH/[0-9]{2}/[0-9]{4}"

for LOG_FILE in $LOG_FILES
do
	# A log file is relevant only if its first entry is in the target month.
	if head -n 1 "$LOG_FILE" | egrep -q "$DATE_REGEX"; then
		DATE=$(head -n 1 "$LOG_FILE" | egrep -o "$DATE_REGEX")
		YEAR=${DATE:7:4}
		MATCHES=$(egrep " $AGENT " "$LOG_FILE")
		if [ -n "$MATCHES" ]; then
			echo "$MATCHES" >> "queryResults/${AGENT}_${MONTH}_$YEAR.log"
		fi
	fi
done
