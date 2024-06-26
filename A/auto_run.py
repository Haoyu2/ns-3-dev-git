import os
import subprocess
from pathlib import Path

import typer

WORKING_DIR = Path("/mnt/c/Users/haoyu/CLionProjects/ns-3-dev-git/build/src/my/example")
TCP_APP = WORKING_DIR / "s3-dev-my-main-example-debug"

def run_tcp_help():
    # Define the command and its arguments
    command = [TCP_APP, '--PrintHelp']

    # Run the command
    result = subprocess.run(command, capture_output=True, text=True)

    # Print the output of the command
    if result.returncode == 0:
        print("Command output:\n", result.stdout)
    else:
        print("Error occurred:\n", result.stderr)


# def run_build():
#     # Define the command and its arguments
#
#     os.chdir("/mnt/c/Users/haoyu/CLionProjects/ns-3-dev-git/cmake-build-debug")
#     print(os.getcwd())
#     # build = "--build /mnt/c/Users/haoyu/CLionProjects/ns-3-dev-git/cmake-build-debug --"
#     command = ["/usr/bin/cmake --build /mnt/c/Users/haoyu/CLionProjects/ns-3-dev-git/cmake-build-debug --target dctcp-example -- -j 22"]
#
#     with subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True) as process:
#         try:
#             # Read the output line by line as it is produced
#             for line in process.stdout:
#                 print(f"Output: {line}", end='')  # end='' prevents adding extra newlines
#
#             # Wait for the process to complete and get the return code
#             returncode = process.wait()
#
#             if returncode != 0:
#                 print(f"Error: Command returned non-zero exit status {returncode}")
#                 # Capture and print the error message
#                 error_message = process.stderr.read()
#                 print(f"Error output: {error_message}")
#         except Exception as e:
#             print(f"Exception occurred: {e}")


def run_command_lively(threshold, numNodes):

    args = f"--MarkingThreshold={threshold} --NumberNodes={numNodes} --DataRateNeck=10Gbps --DataRateLeaf=10Gbps \
--DelayNeck=50us --DelayLeaf=25us --FlowStartupWindow=0s --StopTime=3s --StartRecordingQS=2.55s \
--StopRecordingQS=2.7s --IntervalRecordingQS=1ms"
    # Define the absolute path to the command and its arguments
    command = [TCP_APP, args]

    # Start the process
    with subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True) as process:
        try:
            # Read the output line by line as it is produced
            for line in process.stdout:
                print(f"Output: {line}", end='')  # end='' prevents adding extra newlines

            # Wait for the process to complete and get the return code
            returncode = process.wait()

            if returncode != 0:
                print(f"Error: Command returned non-zero exit status {returncode}")
                # Capture and print the error message
                error_message = process.stderr.read()
                print(f"Error output: {error_message}")
        except Exception as e:
            print(f"Exception occurred: {e}")

def main():
    print(Path.cwd())
    # run_tcp_help()
    # run_build()
    for j in [10, 20, 30, 40, 50, 60, 70]:
        for i in [10, 40]:
            run_command_lively(j, i)

if __name__ == "__main__":

    os.chdir(WORKING_DIR)
    typer.run(main)
