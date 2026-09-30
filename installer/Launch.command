#!/bin/sh
# macOS source-preview launcher. Python 3.10+ with Tk must already be installed.
task_script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd) || exit 1
task_python=''
for task_candidate in python3.15 python3.14 python3.13 python3.12 python3.11 python3.10 python3; do
    if command -v "$task_candidate" >/dev/null 2>&1 && "$task_candidate" -c 'import sys, tkinter; assert sys.version_info >= (3, 10)' >/dev/null 2>&1; then
        task_python=$task_candidate
        break
    fi
done
if [ -z "$task_python" ]; then
    printf '%s\n' 'This preview needs Python 3.10+ with Tk. No files were changed.'
    read -r task_reply
    exit 1
fi
"$task_python" "$task_script_dir/wizard.py"
task_exit=$?
if [ "$task_exit" -ne 0 ]; then
    printf '%s\n' 'Press Return to close.'
    read -r task_reply
fi
exit "$task_exit"
