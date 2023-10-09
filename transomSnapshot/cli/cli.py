import os
import subprocess
import sys


def main():
    cwd = os.path.dirname(os.path.abspath(__file__))
    binary = cwd + "/../transom_snapshot_cli"
    subprocess.call([binary] + sys.argv[1:])


if __name__ == "__main__":
    main()
