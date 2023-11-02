#!/bin/bash
RED='\033[31m'
GREEN='\033[32m'
NC='\033[0m'

INTERVAL=${1:-2}
CHECK_OS="`uname -s`"

if [[ "$CHECK_OS" = "Darwin"* ]]; then
	EXAMPLES=`find . -perm +111 -type f | sort | uniq`
else
	EXAMPLES=`find . -executable -type f | sort | uniq`
fi

for EXAMPLE in $EXAMPLES
do
	if [[ $EXAMPLE == *.sh ]]; then
		continue
	fi

	echo -e "Execute: "${GREEN}$EXAMPLE${NC}" for "$INTERVAL" second(s)"
	$EXAMPLE &
	EXAMPLE_PID=$!
	sleep $INTERVAL
	kill -s SIGTERM $EXAMPLE_PID
	if [ $? -ne 0 ]; then
    		echo -e "Something wrong with: "${RED}$EXAMPLE${NC}
	fi
done
