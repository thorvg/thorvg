from typing import Optional

POSSIBLE_PROBLEM = "POSSIBLE_PROBLEM - "


def check_file(file_name: str) -> Optional[str]:
    with open(file_name, "r") as file:
        print(f"Checking file {file_name}")
        for line in file:
            line = line.strip()
            if line.startswith(POSSIBLE_PROBLEM):
                return line[len(POSSIBLE_PROBLEM) :]
    return None


if __name__ == "__main__":
    data_to_check = [
        (
            "result_valid_files.txt",
            True,
            "Found difference in converting images that properly converted in develop branch.",
        ),
        (
            "result_not_valid_files.txt",
            False,
            "Found differences in converting images that were not properly converted in develop branch(this may be improvement).",
        ),
        (
            "result_image_size.txt",
            True,
            "Generated png have different size in each run.",
        ),
        (
            "result_crashes.txt",
            True,
            "Found crashes in crashes during when converting svg to png.",
        ),
    ]

    fail_ci = False
    comment = ""
    for file_name, fail_ci_when_triggered, new_comment in data_to_check:
        possible_problem = check_file(file_name)
        if possible_problem is not None:
            print(f">>>>> Found changes in {file_name} (look at CI steps from above)")
            print(f"Comment: {new_comment}")

            if fail_ci_when_triggered:
                fail_ci = True
            comment += new_comment + "\n"

    to_write = "Regression report:\n"

    if len(comment) > 0:
        to_write += comment
        to_write += "\nCheck CI for artifacts/logs for more details."
    else:
        to_write += "Not found any changes(both improvements and regressions) in this PR with test data."

    print("\n\n" + to_write + "\n\n")

    with open("comment.txt", "w") as file:
        file.write(to_write)

    if fail_ci:
        with open("fail_ci.txt", "w") as file:
            file.write("Fail")
