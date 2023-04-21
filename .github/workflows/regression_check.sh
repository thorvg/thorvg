#!/bin/bash

if [[ -z "$GITHUB_TOKEN" ]]; then
    echo "The GITHUB_TOKEN is required."
    exit 1
fi

if [[ "$GITHUB_EVENT_NAME" == "pull_request" ]]; then
    pwd

    PAYLOAD_REGRESSION=`cat regression_logs.txt | grep "Regression results:"`
    COMMENTS_URL=$(cat $GITHUB_EVENT_PATH | jq -r .pull_request.comments_url)

    echo $COMMENTS_URL
    echo $PAYLOAD_REGRESSION

    if [[ $PAYLOAD_REGRESSION == *"Regression results:"* ]]; then
        echo "Found broken files"
        OUTPUT+=$'\nFound difference in generated example png files, look into CI artifacts for list of files that was changed.'
        OUTPUT+="$PAYLOAD_REGRESSION"
        OUTPUT+=$'\n'
    fi

    PAYLOAD=$(echo '{}' | jq --arg body "$OUTPUT" '.body = $body')

    curl -s -S -H "Authorization: token $GITHUB_TOKEN" --header "Content-Type: application/vnd.github.VERSION.text+json" --data "$PAYLOAD" "$COMMENTS_URL"
fi

