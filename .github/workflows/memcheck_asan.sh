#!/bin/bash

if [[ -z "$GITHUB_TOKEN" ]]; then
    echo "The GITHUB_TOKEN is required."
    exit 1
fi

if [[ "$GITHUB_EVENT_NAME" == "pull_request" ]]; then
    echo "Run Address Sanitizer"
    echo "meson -Db_sanitize=\"address,undefined\" -Dloaders=\"all\" -Dsavers=\"all\" -Dtests=\"true\" -Dbindings=\"capi\" . build"
    pwd
    cd ${GITHUB_WORKSPACE}/build/test
    ./tvgUnitTests > memcheck_asan.txt 2>&1


    PAYLOAD_MEMCHECK=`cat memcheck_asan.txt`
    COMMENTS_URL=$(cat $GITHUB_EVENT_PATH | jq -r .pull_request.comments_url)

    echo $COMMENTS_URL
    echo "MEMCHECK errors:"
    echo $PAYLOAD_MEMCHECK

    if [[ $PAYLOAD_MEMCHECK == *"runtime error:"* || $PAYLOAD_MEMCHECK == *"ERROR: AddressSanitizer:"* ]]; then
        OUTPUT+=$'\n**MEMCHECK(ASAN) RESULT**:\n'

        OUTPUT+=$'\n`meson -Db_sanitize="address,undefined" -Dloaders="all" -Dsavers="all" -Dtests="true" -Dbindings="capi" . build`\n'
        OUTPUT+=$'\n```\n'
        OUTPUT+="$PAYLOAD_MEMCHECK"
        OUTPUT+=$'\n```\n' 

        PAYLOAD=$(echo '{}' | jq --arg body "$OUTPUT" '.body = $body')

        curl -s -S -H "Authorization: token $GITHUB_TOKEN" --header "Content-Type: application/vnd.github.VERSION.text+json" --data "$PAYLOAD" "$COMMENTS_URL"
    fi
fi

