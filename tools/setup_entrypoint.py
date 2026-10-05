#!/usr/bin/env python3
"""Offline GUI entry point; developer smoke mode only writes an explicit receipt."""
from __future__ import annotations
import argparse
import json
import os
from pathlib import Path
import sys

# The installer directory is an import root, not a Python package.
source_root = Path(getattr(sys, "_MEIPASS", Path(__file__).resolve().parents[1]))
sys.path.insert(0, str(source_root / "installer"))
from wizard import Wizard, main as wizard_main


def main():
    parser = argparse.ArgumentParser(description="NxHalo local game preparation")
    parser.add_argument("--self-test", action="store_true", help="Developer GUI startup check")
    parser.add_argument("--receipt", type=Path, help="New receipt file in an existing directory")
    args = parser.parse_args()
    if not args.self_test:
        if args.receipt is not None:
            parser.error("--receipt requires --self-test")
        wizard_main()
        return
    if args.receipt is None or not args.receipt.is_absolute():
        parser.error("--self-test requires an absolute --receipt")
    receipt = args.receipt
    if not receipt.parent.is_dir() or receipt.exists() or receipt.is_symlink():
        parser.error("receipt must be a new file in an existing directory")
    import tkinter as tk
    root = tk.Tk()
    root.withdraw()
    app = Wizard(root)
    root.update_idletasks()
    root.update()
    result = {
        "schema": 1,
        "result": "pass",
        "scope": "GUI construction and Tk event loop only",
        "first_user_file_chooser_flow": "unverified",
        "game_import": "not performed",
        "frozen": bool(getattr(sys, "frozen", False)),
        "tcl_patchlevel": root.tk.call("info", "patchlevel"),
        "tk_patchlevel": root.tk.call("package", "provide", "Tk"),
        "window_title": root.title(),
        "fields": sorted(app.values),
        "network": "no application network operations",
    }
    root.destroy()
    # Exclusive creation preserves any prior receipt. No automatic output path.
    with receipt.open("x", encoding="utf-8") as out:
        json.dump(result, out, indent=2)
        out.write("\n")


if __name__ == "__main__":
    main()
