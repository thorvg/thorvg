#!/bin/bash

if [[ -z "$GITHUB_TOKEN" ]]; then
    echo "The GITHUB_TOKEN is required."
    exit 1
fi

if [[ "$GITHUB_EVENT_NAME" == "pull_request" ]]; then
    echo "Run Valgrind"
    echo "valgrind --leak-check=yes ./tvgUnitTests"
    cd ./build/test

    valgrind --leak-check=yes ./tvgUnitTests > memcheck_valgrind.txt 2>&1


    PAYLOAD_MEMCHECK=`cat memcheck_valgrind.txt`
    COMMENTS_URL=$(cat $GITHUB_EVENT_PATH | jq -r .pull_request.comments_url)

    echo $COMMENTS_URL
    echo "MEMCHECK errors:"
    echo $PAYLOAD_MEMCHECK

    DEFINITELY_LOST_NUMBER=$(echo "$PAYLOAD_MEMCHECK" | grep -oP 'definitely lost:\s*\K[0-9]+(?=\s*bytes in)')
    ERROR_NUMBER=$(echo "$PAYLOAD_MEMCHECK" | grep -oP 'ERROR SUMMARY:\s*\K[0-9]+(?=\s*errors)')
    if [[ $ERROR_NUMBER != 0 || $DEFINITELY_LOST_NUMBER != 0 || $PAYLOAD_MEMCHECK == *"Invalid read "* || $PAYLOAD_MEMCHECK == *"Invalid write "* ]]; then
        OUTPUT+=$'\n**MEMCHECK(VALGRIND) RESULT**:\n'
        OUTPUT+=$'\n`valgrind --leak-check=yes ./tvgUnitTests`\n'
        OUTPUT+=$'\n```\n'
        OUTPUT+="$PAYLOAD_MEMCHECK"
        OUTPUT+=$'\n```\n' 

        (
            echo '<details><summary>Valgrind output</sumary>'
            echo
            echo "$OUTPUT"
            echo
            echo '</details>'
        ) >> "$GITHUB_STEP_SUMMARY"

        PAYLOAD=$(echo '{}' | jq --arg body "$OUTPUT" '.body = $body')

        curl -s -S -H "Authorization: token $GITHUB_TOKEN" --header "Content-Type: application/vnd.github.VERSION.text+json" --data "$PAYLOAD" "$COMMENTS_URL"
    fi
fi

