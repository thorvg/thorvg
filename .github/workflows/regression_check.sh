#!/bin/bash

if [[ -z "$GITHUB_TOKEN" ]]; then
  echo "The GITHUB_TOKEN is required."
  exit 1
fi

if [[ "$GITHUB_EVENT_NAME" == "pull_request" ]]; then
  pwd

  CREATE_COMMENT=0
  FAIL_CI=0

  COMMENTS_URL=$(cat $GITHUB_EVENT_PATH | jq -r .pull_request.comments_url)
  echo "$COMMENTS_URL"
  POSSIBLE_PROBLEM_SUBSTRING="POSSIBLE_PROBLEM - "

  VALID_FILES=$(cat result_valid_files.txt | grep "POSSIBLE_PROBLEM")
  echo "$VALID_FILES"

  if [[ $VALID_FILES == *"POSSIBLE_PROBLEM"* ]]; then
    echo "Found changed valid files"
    OUTPUT+=$'\nFound regression in converting images that properly converted in develop branch.\n'
    OUTPUT+="${VALID_FILES#$POSSIBLE_PROBLEM_SUBSTRING}"
    OUTPUT+=$'\n'
    CREATE_COMMENT=1
    FAIL_CI=1
  fi

  NOT_VALID_FILES=$(cat result_not_valid_files.txt | grep "POSSIBLE_PROBLEM")
  echo "$NOT_VALID_FILES"

  if [[ $NOT_VALID_FILES == *"POSSIBLE_PROBLEM"* ]]; then
    echo "Found changed non valid files"
    OUTPUT+=$'\nFound differences in converting images that were not properly converted in develop branch.\n'
    OUTPUT+="${NOT_VALID_FILES#$POSSIBLE_PROBLEM_SUBSTRING}"
    OUTPUT+=$'\n'
    CREATE_COMMENT=1
  fi

  IMAGE_SIZE=$(cat result_image_size.txt | grep "POSSIBLE_PROBLEM")
  echo "$IMAGE_SIZE"

  if [[ $IMAGE_SIZE == *"POSSIBLE_PROBLEM"* ]]; then
    echo "Found difference in size generated image"
    OUTPUT+=$'\nGenerated png have different size in each run.\n'
    OUTPUT+="$IMAGE_SIZE"
    OUTPUT+="${IMAGE_SIZE#$POSSIBLE_PROBLEM_SUBSTRING}"
    OUTPUT+=$'\n'
    CREATE_COMMENT=1
    FAIL_CI=1
  fi


  if [ "$FAIL_CI" -eq 1 ]; then
    touch "EXIT_REQUESTED"
  fi

  if [ "$CREATE_COMMENT" -eq 1 ]; then
    PAYLOAD=$(echo '{}' | jq --arg body "$OUTPUT" '.body = $body')
    curl -s -S -H "Authorization: token $GITHUB_TOKEN" --header "Content-Type: application/vnd.github.VERSION.text+json" --data "$PAYLOAD" "$COMMENTS_URL"
  fi

fi
