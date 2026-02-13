#!/usr/bin/env python3
# /// script
# requires-python = ">=3.9"
# ///

import os
import subprocess
import sys


def run_cmd_lines(cmd: str):
    proc = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    assert proc.stdout is not None
    for line in proc.stdout:
        yield line
    proc.wait()


def runexample(directory: str, filename: str) -> None:
    sys.stdout.write("------------------------------------------------------------\n")
    sys.stdout.write(f"Running {filename}\n")
    cmd = f"{directory}/examples/{filename} -time 120000 2>&1"
    for line in run_cmd_lines(cmd):
        sys.stdout.write(line)
    sys.stdout.write("------------------------------------------------------------\n")


def main() -> int:
    directory = sys.argv[1] if len(sys.argv) > 1 else ""
    find_cmd = f"find {directory}/examples -maxdepth 1 -type f ! -name '*.*'"
    for x in run_cmd_lines(find_cmd):
        x = x.rstrip("\n")
        filename = os.path.basename(x)

        prop: list[str] = []
        model: list[str] = []
        for line in run_cmd_lines(f"{x} -help 2>&1"):
            if "-propagation (" in line:
                l1 = line
                l1 = l1.split("-propagation (", 1)[1]
                l1 = l1.split(")", 1)[0]
                l1 = l1.replace(" ", "")
                prop = l1.split(",") if l1 else []
            elif "-model (" in line:
                l1 = line
                l1 = l1.split("-model (", 1)[1]
                l1 = l1.split(")", 1)[0]
                l1 = l1.replace(" ", "")
                model = l1.split(",") if l1 else []

        if len(prop) == 0:
            if len(model) == 0:
                runexample(directory, filename)
            else:
                for m in model:
                    runexample(directory, f"{filename} -model {m}")
        else:
            for p in prop:
                if len(model) == 0:
                    runexample(directory, f"{filename} -propagation {p}")
                else:
                    for m in model:
                        runexample(directory, f"{filename} -propagation {p} -model {m}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
