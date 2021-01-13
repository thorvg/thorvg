#!/bin/bash

if [[ -z "$GITHUB_TOKEN" ]]; then
	echo "The GITHUB_TOKEN is required."
	exit 1
fi

FILES_LINK=`jq -r '.pull_request._links.self.href' "$GITHUB_EVENT_PATH"`/files
echo "Files = $FILES_LINK"

curl $FILES_LINK > files.json
FILES_URLS_STRING=`jq -r '.[].raw_url' files.json`

readarray -t URLS <<<"$FILES_URLS_STRING"

echo "File names: $URLS"

mkdir files
cd files
for i in "${URLS[@]}"
do
   echo "Downloading $i"
   curl -LOk --remote-name $i 
done

echo "Files downloaded!"
echo "Performing checkup:"

cpplint --filter=-,\
+whitespace/parens,\
+whitespace/indent,\
+whitespace/end_of_line,\
+whitespace/blank_line \
--extension=cpp,h,c \
--recursive \
./ > cpp-report.txt 2>&1

PAYLOAD_CPPLINT=`cat cpp-report.txt`
COMMENTS_URL=$(cat $GITHUB_EVENT_PATH | jq -r .pull_request.comments_url)
  
echo $COMMENTS_URL
echo "Cppcheck errors:"
echo $PAYLOAD_CPPLINT

if [[ $PAYLOAD_CPPLINT == *"Total errors found: "* ]]; then
  OUTPUT+=$'\n**CODING STYLE CHECK**:\n'
  OUTPUT+=$'\n```\n'
  OUTPUT+="$PAYLOAD_CPPLINT"
  OUTPUT+=$'\n```\n' 

else
  OUTPUT+=$'\n**CODING STYLE CHECK**:\n'
  OUTPUT+=$'\n```\n'
  OUTPUT+="Perfect."
  OUTPUT+=$'\n```\n' 
fi
PAYLOAD=$(echo '{}' | jq --arg body "$OUTPUT" '.body = $body')

curl -s -S -H "Authorization: token $GITHUB_TOKEN" --header "Content-Type: application/vnd.github.VERSION.text+json" --data "$PAYLOAD" "$COMMENTS_URL"
