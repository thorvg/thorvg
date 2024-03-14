import os
import json
import subprocess

GITHUB_TOKEN = os.getenv("GITHUB_TOKEN")
GITHUB_EVENT_NAME = os.getenv("GITHUB_EVENT_NAME")
GITHUB_EVENT_PATH = os.getenv("GITHUB_EVENT_PATH") or "Missing"

RUST_PANIC = "panicked"
POSSIBLE_PROBLEM = "POSSIBLE_PROBLEM - "


def check_github_token() -> None:
    if not GITHUB_TOKEN:
        print("The GITHUB_TOKEN is required.")
        exit(1)


def check_file(file_name: str) -> tuple[list[str], list[str]]:
    panics = []
    possible_problems = []
    with open(file_name, "r") as file:
        print(f"Checking file {file_name}")
        for line in file:
            line = line.strip()
            if RUST_PANIC in line:
                panics.append(line)
            elif POSSIBLE_PROBLEM in line:
                possible_problems.append(line)

    print(f"Panics in {file_name}", panics)
    print(f"Possible problems in {file_name}", possible_problems)
    return panics, possible_problems


def check_changes_in_valid_files() -> tuple[bool, str]:
    panics, possible_problems = check_file("result_valid_files.txt")

    if len(panics) != 0 or len(possible_problems) != 0:
        print(">>>>> Found changed in previously valid files (see above)")

        github_comment = "\nFound regression in converting images that properly converted in develop branch.\n"
        for line in panics:
            github_comment += (
                line.replace(RUST_PANIC, "") + " - panicked when checking\n"
            )
        for line in possible_problems:
            github_comment += (
                line.replace(POSSIBLE_PROBLEM, "") + " - changed visually \n"
            )
        github_comment += "\n"

        return True, github_comment
    return False, ""


def check_changes_in_invalid_files() -> tuple[bool, str]:
    panics, possible_problems = check_file("result_not_valid_files.txt")

    if len(panics) != 0 or len(possible_problems) != 0:
        print(">>>>> Found changed non valid files (see above)")
        for line in panics + possible_problems:
            print(line)

        github_comment = "\nFound differences in converting images that were not properly converted in develop branch.\n"
        for line in panics:
            github_comment += (
                line.replace(RUST_PANIC, "") + " - panicked when checking\n"
            )
        for line in possible_problems:
            github_comment += (
                line.replace(POSSIBLE_PROBLEM, "") + " - changed visually \n"
            )
        github_comment += "\n"

        return False, github_comment
    return False, ""


def check_changes_in_image_size() -> tuple[bool, str]:
    panics, possible_problems = check_file("result_image_size.txt")

    if len(panics) != 0 or len(possible_problems) != 0:
        print(">>>>> Found difference in size generated image(see above)")
        for line in panics + possible_problems:
            print(line)

        github_comment = "\nGenerated png have different size in each run.\n"
        for line in panics:
            github_comment += (
                line.replace(RUST_PANIC, "") + " - panicked when checking\n"
            )
        for line in possible_problems:
            github_comment += (
                line.replace(POSSIBLE_PROBLEM, "") + " - changed visually \n"
            )
        github_comment += "\n"

        return True, github_comment
    return False, ""


def send_comment(github_comment: str) -> None:
    with open(GITHUB_EVENT_PATH, "r") as file:
        data = json.load(file)
        comments_url = data["pull_request"]["comments_url"]

    print(f"Sending comment to {comments_url}")

    payload = json.dumps({"body": github_comment})
    curl_command = f'curl -s -S -H "Authorization: token {GITHUB_TOKEN}" --header "Content-Type: application/vnd.github.VERSION.text+json" --data "{payload}" "{comments_url}"'
    # Print output from curl
    subprocess.run(curl_command, shell=True)


if __name__ == "__main__":
    if GITHUB_EVENT_NAME != "pull_request":
        print(
            "You are running this script in wrong event - expected pull_request, crashes may occur"
        )

    print(os.getcwd())

    print()
    fail_ci_v, comment_v = check_changes_in_valid_files()
    print()
    fail_ci_i, comment_i = check_changes_in_invalid_files()
    print()
    fail_ci_s, comment_s = check_changes_in_image_size()
    print()

    fail_ci = fail_ci_v or fail_ci_i or fail_ci_s
    comment = (comment_v + comment_i + comment_s).strip()

    if GITHUB_EVENT_NAME == "pull_request":
        if len(comment) > 0:
            send_comment(comment)

    if fail_ci:
        raise Exception("Found regression in image conversion")
