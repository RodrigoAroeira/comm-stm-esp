"""Merge each PlatformIO environment's compilation database for clangd.

pioarduino's clangd backend runs

    pio run --target compiledb --environment <active-env>

which only ever builds the *active* environment into ``compile_commands.json``.
The extension then fixes that up and writes ``.cache/clangd/compile_commands.json``
(the database clangd reads), synthesizing entries for the remaining project
files with the *active* environment's flags. For a project with two different
CPU families (ESP32 + STM32) those synthetic flags are wrong for the other
target's files.

This script is loaded as a PlatformIO ``post`` extra script. When the
``compiledb`` target runs, it appends the freshly generated entries to
``.cache/clangd/compile_commands.json``, each keeping its *own* environment's
flags, and preserves the entries of the other environments.

It runs via ``atexit`` so it happens after PlatformIO has written the raw
database, and it timestamps the merged file newer than the raw one so the
pioarduino extension's own fixup (which only triggers when the raw database is
newer) leaves our merged database in place.
"""

import atexit
import json
import os
import time

Import("env")  # noqa: F821 - provided by SCons/PlatformIO

from SCons.Script import COMMAND_LINE_TARGETS  # noqa: E402

PROJECT_DIR = env.subst("$PROJECT_DIR")  # noqa: F821
RAW_DB = os.path.join(PROJECT_DIR, "compile_commands.json")
CLANGD_DB_DIR = os.path.join(PROJECT_DIR, ".cache", "clangd")
CLANGD_DB = os.path.join(CLANGD_DB_DIR, "compile_commands.json")


def _absolutize(entry):
    """Make ``file``/``directory`` absolute so clangd resolves them anywhere."""
    directory = entry.get("directory") or PROJECT_DIR
    entry["directory"] = os.path.abspath(directory)
    path = entry.get("file")
    if path:
        if not os.path.isabs(path):
            path = os.path.join(entry["directory"], path)
        entry["file"] = os.path.normpath(path)
    return entry


def _load(path):
    try:
        with open(path, "r", encoding="utf-8") as handle:
            return json.load(handle)
    except (OSError, ValueError):
        return []


def merge_clangd_db():
    if not os.path.isfile(RAW_DB):
        return
    raw = _load(RAW_DB)
    if not raw:
        return

    merged = {}
    for entry in _load(CLANGD_DB):
        if entry.get("file"):
            merged[os.path.normpath(entry["file"])] = entry
    for entry in raw:
        entry = _absolutize(entry)
        if entry.get("file"):
            merged[os.path.normpath(entry["file"])] = entry

    try:
        os.makedirs(CLANGD_DB_DIR, exist_ok=True)
        with open(CLANGD_DB, "w", encoding="utf-8") as handle:
            json.dump(list(merged.values()), handle, indent=1)
        now = time.time()
        os.utime(CLANGD_DB, (now, now))
        print("clangd: merged %d entries into %s" % (len(merged), CLANGD_DB))
    except OSError as error:
        print("clangd: merge failed: %s" % error)


if "compiledb" in COMMAND_LINE_TARGETS:
    atexit.register(merge_clangd_db)
