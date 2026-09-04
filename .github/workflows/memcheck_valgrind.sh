#!/bin/bash

if [[ -z "$GITHUB_TOKEN" ]]; then
    echo "The GITHUB_TOKEN is required."
    exit 1
fi

if [[ "$GITHUB_EVENT_NAME" == "pull_request" ]]; then
    VALGRIND_SUPPRESSIONS="${GITHUB_WORKSPACE}/.github/workflows/valgrind.supp"
    VALGRIND_CMD="env LIBGL_ALWAYS_SOFTWARE=1 LP_NUM_THREADS=1 valgrind --suppressions=$VALGRIND_SUPPRESSIONS --leak-check=yes ./tvgUnitTests"
    echo "Base command: $VALGRIND_CMD"
    echo "Run Valgrind in 4 parallel shards"
    cd ./build/test || exit 1

    pids=()
    logs=()
    run_shard() {
        local name="$1"
        shift
        local log="memcheck_valgrind_${name}.txt"
        # LP_NUM_THREADS limits Mesa llvmpipe rendering threads, not OpenMP threads.
        env LIBGL_ALWAYS_SOFTWARE=1 LP_NUM_THREADS=1 valgrind --suppressions="$VALGRIND_SUPPRESSIONS" --leak-check=yes ./tvgUnitTests "$@" > "$log" 2>&1 &
        pids+=("$!")
        logs+=("$log")
    }

    run_shard cpu_gl --test-suite-exclude="[tvgWgCanvas],[tvgWgEngine]" --test-case-exclude="GL Image Draw"
    run_shard gl_image_wg_canvas --test-suite="[tvgGlEngine],[tvgWgCanvas]" --test-case="GL Image Draw,WG *"
    run_shard wg_image_rotate --test-suite="[tvgWgEngine]" --test-case="WG Image Draw,WG Image Rotation"
    run_shard wg_rest --test-suite="[tvgWgEngine]" --test-case-exclude="WG Image Draw,WG Image Rotation"

    rc=0
    for pid in "${pids[@]}"; do
        wait "$pid" || rc=1
    done

    SHARD_COUNT=${#pids[@]}
    PAYLOAD_MEMCHECK=$(cat "${logs[@]}")
    COMMENTS_URL=$(jq -r .pull_request.comments_url "$GITHUB_EVENT_PATH")

    echo "$COMMENTS_URL"
    echo "MEMCHECK errors:"
    echo "$PAYLOAD_MEMCHECK"

    DEFINITELY_LOST_NUMBER=$(echo "$PAYLOAD_MEMCHECK" | grep -oP 'definitely lost:\s*\K[0-9,]+(?=\s*bytes in)' | awk '{gsub(",", "", $1); sum += $1} END {print sum + 0}')
    ERROR_NUMBER=$(echo "$PAYLOAD_MEMCHECK" | grep -oP 'ERROR SUMMARY:\s*\K[0-9,]+(?=\s*errors)' | awk '{gsub(",", "", $1); sum += $1} END {print sum + 0}')
    if [[ $rc != 0 || $ERROR_NUMBER != 0 || $DEFINITELY_LOST_NUMBER != 0 || $PAYLOAD_MEMCHECK == *"Invalid read "* || $PAYLOAD_MEMCHECK == *"Invalid write "* ]]; then
        OUTPUT+=$'\n**MEMCHECK(VALGRIND) RESULT**:\n'
        OUTPUT+=$'\n'"$SHARD_COUNT"$' parallel shards using `'"$VALGRIND_CMD"$'`\n'
        OUTPUT+=$'\n```\n'
        OUTPUT+="$PAYLOAD_MEMCHECK"
        OUTPUT+=$'\n```\n'

        (
            echo '<details><summary>Valgrind output</summary>'
            echo
            echo "$OUTPUT"
            echo
            echo '</details>'
        ) >> "$GITHUB_STEP_SUMMARY"

        PAYLOAD=$(echo '{}' | jq --arg body "$OUTPUT" '.body = $body')

        curl -s -S -H "Authorization: token $GITHUB_TOKEN" --header "Content-Type: application/vnd.github.VERSION.text+json" --data "$PAYLOAD" "$COMMENTS_URL"
    fi

    if (( rc != 0 )); then
        echo "One or more Valgrind jobs failed." >&2
        exit 1
    fi
fi
