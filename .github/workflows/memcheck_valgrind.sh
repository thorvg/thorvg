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
    PAYLOAD_MEMCHECK=
    for i in "${!pids[@]}"; do
        shard_rc=0
        wait "${pids[$i]}" || { rc=1; shard_rc=1; }

        if [[ $shard_rc != 0 ]] || grep -Eq 'ERROR SUMMARY:[[:space:]]*[1-9][0-9,]*[[:space:]]+errors|definitely lost:[[:space:]]*[1-9][0-9,]*[[:space:]]+bytes in|Invalid (read|write) ' "${logs[$i]}"; then
            PAYLOAD_MEMCHECK+="$(<"${logs[$i]}")"$'\n'
        fi
    done

    SHARD_COUNT=${#pids[@]}
    COMMENTS_URL=$(jq -r .pull_request.comments_url "$GITHUB_EVENT_PATH")

    echo "$COMMENTS_URL"
    echo "MEMCHECK results:"
    cat "${logs[@]}"

    if [[ -n $PAYLOAD_MEMCHECK ]]; then
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
