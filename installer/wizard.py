#!/usr/bin/env python3
"""Local preparation wizard. Requires Python 3.10+ with Tk; performs no downloads."""
from __future__ import annotations

import os
from pathlib import Path
import queue
import shutil
import subprocess
import sys
import threading
import time

from installer import InstallerError, build_installer, export_data, find_packer, inspect_game, prepare_sd

try:
    import tkinter as tk
    from tkinter import filedialog, messagebox, ttk
except ImportError:
    raise SystemExit("This preview needs Python 3.10+ with Tk. Install a Python/Tk package for your computer, or use installer.py from a terminal. No files were changed.")


class Wizard:
    def __init__(self, root):
        self.root = root
        root.title("NxHalo — Prepare your game")
        root.minsize(680, 570)
        root.protocol("WM_DELETE_WINDOW", self.close)
        self.events = queue.Queue()
        self.busy = False
        self.output_ready = None
        self.controls = []
        self.values = {key: tk.StringVar() for key in ("game", "runtime-kit", "keys", "packer", "workspace", "output", "sd-root")}
        self.status = tk.StringVar(value="Choose your own Xbox game image or complete extracted game folder.")
        self.space = tk.StringVar(value="Work files stay on the disk you choose.")
        outer = ttk.Frame(root, padding=22)
        outer.pack(fill="both", expand=True)
        ttk.Label(outer, text="Prepare your Halo CE game", font=("TkDefaultFont", 19, "bold")).pack(anchor="w")
        ttk.Label(outer, text="Choose your own game, prepare its maps, then play from HOME. Nothing is uploaded.", wraplength=620).pack(anchor="w", pady=(8, 18))

        game = ttk.LabelFrame(outer, text="1. Choose your game", padding=12)
        game.pack(fill="x", pady=(0, 12))
        self._entry(game, "game")
        row = ttk.Frame(game)
        row.pack(fill="x", pady=(8, 0))
        self._button(row, "Choose Xbox image…", self.choose_image).pack(side="left", padx=(0, 8))
        self._button(row, "Choose complete game folder…", lambda: self.choose_directory("game")).pack(side="left", padx=(0, 8))
        self._button(row, "Check game", self.check_game).pack(side="left")
        ttk.Label(game, text="Original Xbox Halo data only. The tool checks all 24 maps before building.", wraplength=610).pack(anchor="w", pady=(8, 0))

        notebook = ttk.Notebook(outer, height=350)
        notebook.pack(fill="both", expand=True, pady=(0, 12))
        data_tab = ttk.Frame(notebook, padding=12)
        advanced_holder = ttk.Frame(notebook)
        advanced_canvas = tk.Canvas(advanced_holder, highlightthickness=0)
        advanced_scroll = ttk.Scrollbar(advanced_holder, orient="vertical", command=advanced_canvas.yview)
        advanced_canvas.configure(yscrollcommand=advanced_scroll.set)
        advanced_scroll.pack(side="right", fill="y")
        advanced_canvas.pack(side="left", fill="both", expand=True)
        advanced_tab = ttk.Frame(advanced_canvas, padding=12)
        advanced_window = advanced_canvas.create_window((0, 0), window=advanced_tab, anchor="nw")
        advanced_tab.bind("<Configure>", lambda event: advanced_canvas.configure(scrollregion=advanced_canvas.bbox("all")))
        advanced_canvas.bind("<Configure>", lambda event: advanced_canvas.itemconfigure(advanced_window, width=event.width))
        advanced_canvas.bind_all("<MouseWheel>", lambda event: advanced_canvas.yview_scroll(-int(event.delta / 120 or event.delta), "units"))
        notebook.add(data_tab, text="Prepare game (recommended)")
        notebook.add(advanced_holder, text="Full installer (advanced)")
        ttk.Label(data_tab, text="Use the separate, map-free runtime installer through DBI. This source preview does not include it. Preparing game data requires no private keys or packer.", wraplength=600).pack(anchor="w", pady=(0, 14))
        self._field(data_tab, "Choose the mounted SD root (optional)", "sd-root", lambda: self.choose_directory("sd-root"))
        ttk.Label(data_tab, text="Select the root containing the SD's switch folder. Existing different maps are preserved; saves and settings stay untouched. This tool never ejects a drive.", wraplength=600).pack(anchor="w", pady=(0, 8))
        self._button(data_tab, "Prepare selected SD", self.prepare_selected_sd).pack(anchor="w", pady=(0, 18))
        self._field(data_tab, "Or save portable game data in a new folder", "output", self.choose_output)
        self._button(data_tab, "Make portable data folder", self.export_game_data).pack(anchor="w")
        ttk.Label(data_tab, text="Copy that output's maps folder to switch/halo/maps later. Keep your generated game-data folder private.", wraplength=600).pack(anchor="w", pady=(10, 0))

        setup = ttk.LabelFrame(advanced_tab, text="Local full-installer setup", padding=12)
        setup.pack(fill="x", pady=(0, 12))
        self._field(setup, "Private prod.keys", "keys", lambda: self.choose_file("keys", "Choose your private prod.keys", [("Private keys", "*.keys"), ("All files", "*")]))
        ttk.Label(setup, text="Used by your local packer; never copied into the installer or receipt.", wraplength=610).pack(anchor="w", pady=(0, 8))
        self._field(setup, "Save installer in a new folder", "output", self.choose_output)
        self._field(setup, "Work folder / disk", "workspace", lambda: self.choose_directory("workspace"))
        ttk.Label(setup, textvariable=self.space, wraplength=610).pack(anchor="w", pady=(0, 8))

        advanced = ttk.LabelFrame(advanced_tab, text="Runtime and packaging tool", padding=12)
        advanced.pack(fill="x", pady=(0, 14))
        self._field(advanced, "Runtime kit folder", "runtime-kit", lambda: self.choose_directory("runtime-kit"))
        self._field(advanced, "Local hacbrewpack", "packer", lambda: self.choose_file("packer", "Choose a local hacbrewpack executable", [("All files", "*")]))
        ttk.Label(advanced, text="These fill automatically when supplied alongside this preview. Otherwise choose their local locations.", wraplength=610).pack(anchor="w")

        self._button(advanced_tab, "Build full NSP (advanced)", self.build).pack(anchor="w", pady=(0, 10))
        self.progress = ttk.Progressbar(outer, mode="indeterminate")
        self.progress.pack(fill="x", pady=(0, 10))
        ttk.Label(outer, textvariable=self.status, wraplength=630).pack(anchor="w")
        self.open_button = ttk.Button(outer, text="Open finished output", command=self.open_output, state="disabled")
        self.open_button.pack(anchor="w", pady=(10, 0))
        here = Path(__file__).resolve().parent
        for candidate in (here / "runtime-kit", here.parent / "runtime-kit"):
            if (candidate / "runtime-manifest.json").is_file():
                self.values["runtime-kit"].set(str(candidate))
                break
        try:
            self.values["packer"].set(str(find_packer()))
        except InstallerError:
            pass
        root.after(100, self.poll)

    def _button(self, parent, text, command):
        button = ttk.Button(parent, text=text, command=command)
        self.controls.append(button)
        return button

    def _entry(self, parent, key):
        entry = ttk.Entry(parent, textvariable=self.values[key])
        entry.pack(fill="x")
        self.controls.append(entry)

    def _field(self, parent, label, key, command):
        ttk.Label(parent, text=label).pack(anchor="w")
        row = ttk.Frame(parent)
        row.pack(fill="x", pady=(3, 9))
        entry = ttk.Entry(row, textvariable=self.values[key])
        entry.pack(side="left", fill="x", expand=True, padx=(0, 8))
        self.controls.append(entry)
        self._button(row, "Choose…", command).pack(side="right")

    def choose_file(self, key, title, filetypes):
        result = filedialog.askopenfilename(parent=self.root, title=title, filetypes=filetypes)
        if result:
            self.values[key].set(result)

    def choose_image(self):
        self.choose_file("game", "Choose your Xbox game image", [("Xbox disc image", "*.iso *.xiso"), ("All files", "*")])

    def choose_directory(self, key):
        result = filedialog.askdirectory(parent=self.root, title="Choose " + key.replace("-", " "), mustexist=True)
        if result:
            self.values[key].set(result)
            if key == "workspace":
                self.refresh_space()

    def choose_output(self):
        result = filedialog.askdirectory(parent=self.root, title="Choose where to save the new installer folder", mustexist=True)
        if result:
            self.values["output"].set(str(Path(result) / time.strftime("Halo CE Installer %Y-%m-%d %H%M%S")))
            if not self.values["workspace"].get():
                self.values["workspace"].set(result)
                self.refresh_space()

    def refresh_space(self):
        try:
            free = shutil.disk_usage(self.values["workspace"].get()).free / 1024**3
            self.space.set(f"{free:.1f} GiB available. The build checks exact headroom before copying.")
        except OSError:
            self.space.set("The selected work disk cannot be read.")

    def start(self, operation):
        self.busy = True
        self.output_ready = None
        self.open_button.configure(state="disabled")
        for control in self.controls:
            control.configure(state="disabled")
        self.progress.start(12)

        def worker():
            try:
                result = operation()
                self.events.put(("complete", result))
            except InstallerError as error:
                self.events.put(("error", str(error)))
            except Exception:
                # Do not display exception reprs: some contain private input paths.
                self.events.put(("error", "The local operation stopped unexpectedly. Your original files and earlier installers remain unchanged. Retry from a terminal for a reproducible test."))
        threading.Thread(target=worker, name="nxhalo-installer", daemon=False).start()

    def check_game(self):
        value = self.values["game"].get().strip()
        if not value:
            messagebox.showinfo("Choose a game", "Choose your Xbox game image or complete game folder first.", parent=self.root)
            return
        self.status.set("Checking the game's map set and headers…")
        self.start(lambda: ("inspection", inspect_game(Path(value)).summary()))

    def build(self):
        values = {key: self.values[key].get().strip() for key in ("game", "runtime-kit", "keys", "packer", "workspace", "output")}
        missing = [key.replace("-", " ") for key, value in values.items() if not value]
        if missing:
            messagebox.showinfo("Complete local setup", "Choose: " + ", ".join(missing) + ".", parent=self.root)
            return
        self.status.set("Starting local checks…")
        def operation():
            result = build_installer(Path(values["game"]), Path(values["runtime-kit"]), Path(values["keys"]),
                                     Path(values["workspace"]), Path(values["output"]), values["packer"],
                                     lambda text: self.events.put(("status", text)))
            return "build", (result, values["output"])
        self.start(operation)

    def export_game_data(self):
        game, output = (self.values[key].get().strip() for key in ("game", "output"))
        if not game or not output:
            messagebox.showinfo("Choose game and output", "Choose your game and a new output folder first.", parent=self.root)
            return
        self.status.set("Preparing your game data locally…")
        self.start(lambda: ("data", (export_data(Path(game), Path(output),
                          lambda text: self.events.put(("status", text))), output)))

    def prepare_selected_sd(self):
        game, sd_root = (self.values[key].get().strip() for key in ("game", "sd-root"))
        if not game or not sd_root:
            messagebox.showinfo("Choose game and SD", "Choose your game and the mounted SD's actual root folder first. No drive is selected automatically.", parent=self.root)
            return
        self.status.set("Checking your selected game and SD folders…")
        self.start(lambda: ("sd", (prepare_sd(Path(game), Path(sd_root),
                          lambda text: self.events.put(("status", text))), str(Path(sd_root) / "switch/halo/maps"))))

    def poll(self):
        try:
            while True:
                kind, value = self.events.get_nowait()
                if kind == "status":
                    self.status.set(value)
                    continue
                self.busy = False
                self.progress.stop()
                for control in self.controls:
                    control.configure(state="normal")
                if kind == "error":
                    self.status.set("Stopped. No new installer is ready.")
                    messagebox.showerror("Build stopped", value, parent=self.root)
                elif value[0] == "inspection":
                    summary = value[1]
                    self.status.set(f"All {summary['map_count']} required map headers match Xbox build {summary['game_build']}. Complete local setup, then build.")
                elif value[0] == "build":
                    receipt, output = value[1]
                    self.output_ready = output
                    self.open_button.configure(state="normal")
                    self.status.set(f"Ready: Halo_CE.nsp ({receipt['installer']['bytes'] / 1024**3:.2f} GiB). Install through DBI, then launch Halo CE from HOME. Keep your earlier installer until you test this one.")
                elif value[0] == "data":
                    receipt, output = value[1]
                    self.output_ready = output
                    self.open_button.configure(state="normal")
                    self.status.set("Verified game data ready. Copy the maps folder to switch/halo/maps, install the separate runtime NSP with DBI, then launch Halo CE from HOME.")
                elif value[0] == "sd":
                    receipt, output = value[1]
                    self.output_ready = output
                    self.open_button.configure(state="normal")
                    action = "already matched; nothing changed" if receipt["operation"] == "already-matches" else "prepared and hash-checked"
                    self.status.set("SD maps " + action + ". Install the separate runtime NSP with DBI, then launch from HOME. The SD is still connected.")
        except queue.Empty:
            pass
        self.root.after(100, self.poll)

    def open_output(self):
        if not self.output_ready:
            return
        try:
            if sys.platform == "darwin":
                subprocess.Popen(["open", self.output_ready])
            elif os.name == "nt":
                os.startfile(self.output_ready)
            else:
                subprocess.Popen(["xdg-open", self.output_ready], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except OSError:
            messagebox.showinfo("Output ready", "The installer is in the output folder you selected.", parent=self.root)

    def close(self):
        if self.busy:
            messagebox.showinfo("Work is in progress", "Keep this window and the selected disks connected until the check or build finishes. Packing can take several minutes.", parent=self.root)
        else:
            self.root.destroy()


def main():
    if sys.version_info < (3, 10):
        raise SystemExit("This source preview requires Python 3.10 or newer.")
    try:
        root = tk.Tk()
    except tk.TclError:
        raise SystemExit("The wizard needs a graphical desktop and Python with Tk. Use installer.py for terminal operation. No files were changed.") from None
    Wizard(root)
    root.mainloop()


if __name__ == "__main__":
    main()
