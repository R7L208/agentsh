#!/bin/bash
#
# usage.sh - count occurrences of a keyword across the audit logs.
#
# Counts keyword occurrences for quick telemetry over the sandbox audit trail,
# e.g. how many commands were denied:
#
#   ./usage.sh DENY audit      # count all denied command attempts
#   ./usage.sh mkdir audit     # count how often mkdir was invoked
#
# Usage: ./usage.sh <keyword> <audit dir>

if [ "$#" -ne 2 ]; then
	echo "Usage: ./usage.sh <keyword> <audit dir>"
	exit 1
fi

KEYWORD=$1
AUDIT_DIR=$2

count=0
for FILE in $(find "$AUDIT_DIR" -type f)
do
	count=$((count + $(egrep -o "$KEYWORD" "$FILE" | wc -l | tr -d " ") ))
done

echo "$count"
