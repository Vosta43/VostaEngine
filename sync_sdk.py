#!/usr/bin/env python3
"""Mirror the engine SDK into a game project's engine/ folder.

Double-click friendly: prompts for the project directory, then copies
<engine>/SDK/{include,lib,bin,buildsystem,VostaEngine,docs} into
<project>/engine/, removing files that no longer exist in the SDK.
"""

import argparse
import shutil
import sys
import time
from pathlib import Path

ENGINE_ROOT = Path(__file__).resolve().parent
SDK_ROOT = ENGINE_ROOT / "SDK"

PAYLOAD_DIRS = ["include", "lib", "bin", "buildsystem", "VostaEngine", "docs"]

REQUIRED = [
    "buildsystem/Vosta.GameModule.props",
    "lib/VostaEngine.lib",
    "bin/VostaEngine.dll",
    "bin/VostaPlayer.exe",
]

# (SDK-relative artifact, repo build output) - warn when the SDK is older.
FRESHNESS = [
    ("bin/VostaEngine.dll", "VostaEngine/x64/Release/VostaEngine.dll"),
    ("lib/VostaEngine.lib", "VostaEngine/x64/Release/VostaEngine.lib"),
    ("bin/VostaPlayer.exe", "VostaPlayer/x64/Release/VostaPlayer.exe"),
]


def fmt_time(ts: float) -> str:
    return time.strftime("%Y-%m-%d %H:%M", time.localtime(ts))


def file_stamp(path: Path) -> str:
    try:
        st = path.stat()
    except OSError:
        return "missing"
    return f"{fmt_time(st.st_mtime)}  {st.st_size / 1024 / 1024:.2f} MB"


def check_sdk() -> bool:
    if not SDK_ROOT.is_dir():
        print(f"[error] SDK directory not found: {SDK_ROOT}")
        print("        Build the VostaEngineSdk project in VostaEngine/VostaEngine.slnx first.")
        return False
    missing = [rel for rel in REQUIRED if not (SDK_ROOT / rel).is_file()]
    if missing:
        print("[error] SDK is incomplete, missing:")
        for rel in missing:
            print(f"        SDK/{rel}")
        print("        Build the VostaEngineSdk project in VostaEngine/VostaEngine.slnx again.")
        return False
    return True


def warn_if_stale() -> None:
    stale = []
    for sdk_rel, build_rel in FRESHNESS:
        sdk_file = SDK_ROOT / sdk_rel
        build_file = ENGINE_ROOT / build_rel
        if build_file.is_file() and sdk_file.stat().st_mtime < build_file.stat().st_mtime:
            stale.append((sdk_rel, build_rel))
    if stale:
        print("[warning] SDK is older than the repo build output:")
        for sdk_rel, build_rel in stale:
            print(f"          SDK/{sdk_rel}  {fmt_time((SDK_ROOT / sdk_rel).stat().st_mtime)}")
            print(f"          {build_rel}  {fmt_time((ENGINE_ROOT / build_rel).stat().st_mtime)}")
        print("          Build VostaEngineSdk in VS before syncing.\n")


def resolve_engine_dir(raw: str) -> Path | None:
    target = Path(raw.strip().strip('"').strip("'")).expanduser()
    if not target:
        return None
    if not target.is_absolute():
        target = (Path.cwd() / target).resolve()
    if not target.is_dir():
        print(f"[error] directory does not exist: {target}")
        return None

    if (target / "buildsystem" / "Vosta.GameModule.props").is_file():
        return target
    if (target / "project.json").is_file():
        # A missing or partial engine/ is fine: the sync fills it in.
        return target / "engine"

    print(f"[error] {target} is neither a project root (project.json)")
    print("        nor a project's engine/ folder (buildsystem/Vosta.GameModule.props).")
    return None


def find_locked_files(engine_dir: Path) -> list[Path]:
    locked = []
    bin_dir = engine_dir / "bin"
    if not bin_dir.is_dir():
        return locked
    for p in bin_dir.iterdir():
        if p.suffix.lower() not in (".dll", ".exe") or not p.is_file():
            continue
        try:
            with open(p, "r+b"):
                pass
        except PermissionError:
            locked.append(p)
        except OSError:
            pass
    return locked


def plan_mirror(engine_dir: Path) -> tuple[list[Path], list[Path], list[Path]]:
    """Return (new, changed, stale) paths relative to the engine dir."""
    new_files: list[Path] = []
    changed: list[Path] = []
    stale: list[Path] = []
    for name in PAYLOAD_DIRS:
        src_dir = SDK_ROOT / name
        dst_dir = engine_dir / name
        if not src_dir.is_dir():
            continue
        src_files = {p.relative_to(src_dir): p for p in src_dir.rglob("*") if p.is_file()}
        if dst_dir.is_dir():
            dst_files = {p.relative_to(dst_dir) for p in dst_dir.rglob("*") if p.is_file()}
            stale.extend(dst_dir / rel for rel in dst_files - src_files.keys())
        for rel, src in src_files.items():
            dst = dst_dir / rel
            if not dst.is_file():
                new_files.append(dst.relative_to(engine_dir))
            else:
                s, d = src.stat(), dst.stat()
                if s.st_size != d.st_size or int(s.st_mtime) != int(d.st_mtime):
                    changed.append(dst.relative_to(engine_dir))
    return new_files, changed, stale


def mirror(engine_dir: Path) -> None:
    for name in PAYLOAD_DIRS:
        src_dir = SDK_ROOT / name
        dst_dir = engine_dir / name
        if src_dir.is_dir():
            shutil.copytree(src_dir, dst_dir, dirs_exist_ok=True)


def prune(engine_dir: Path) -> int:
    removed = 0
    for name in PAYLOAD_DIRS:
        src_dir = SDK_ROOT / name
        dst_dir = engine_dir / name
        if not src_dir.is_dir() or not dst_dir.is_dir():
            continue
        src_files = {p.relative_to(src_dir) for p in src_dir.rglob("*") if p.is_file()}
        for p in sorted(dst_dir.rglob("*"), key=lambda q: len(q.parts), reverse=True):
            if p.is_file() and p.relative_to(dst_dir) not in src_files:
                p.unlink()
                removed += 1
            elif p.is_dir():
                try:
                    p.rmdir()
                except OSError:
                    pass
    return removed


def sync(engine_dir: Path, assume_yes: bool) -> bool:
    new_files, changed, stale = plan_mirror(engine_dir)

    print(f"\nProject engine: {engine_dir}")
    print(f"SDK engine DLL : {file_stamp(SDK_ROOT / 'bin' / 'VostaEngine.dll')}")
    print(f"Local engine DLL: {file_stamp(engine_dir / 'bin' / 'VostaEngine.dll')}")
    print(f"Pending        : {len(new_files)} new, {len(changed)} changed, {len(stale)} removed")

    if not (new_files or changed or stale):
        print("\nAlready up to date.")
        return True

    if stale:
        print("\nWill be deleted from engine/ (no longer in the SDK):")
        for rel in stale[:10]:
            print(f"  - {rel}")
        if len(stale) > 10:
            print(f"  ... and {len(stale) - 10} more")

    if not assume_yes:
        answer = input("\nContinue? [Y/n] ").strip().lower()
        if answer in ("n", "no"):
            print("Cancelled.")
            return False

    locked = find_locked_files(engine_dir)
    if locked:
        print("\n[error] These files are in use. Close the running game/editor first:")
        for p in locked:
            print(f"        {p.name}")
        return False

    engine_dir.mkdir(parents=True, exist_ok=True)
    try:
        mirror(engine_dir)
        removed = prune(engine_dir)
    except PermissionError as e:
        print(f"\n[error] copy failed (file in use?): {e}")
        print("        Close the running game/editor and retry.")
        return False

    print(f"\nDone: {len(new_files) + len(changed)} files copied, {removed} stale files removed.")
    print("Headers changed - rebuild the game module (the project's .slnx in VS).")
    return True


def main() -> int:
    parser = argparse.ArgumentParser(description="Sync the engine SDK into a game project's engine/ folder")
    parser.add_argument("paths", nargs="*", help="project root or its engine/ folder (one or more)")
    parser.add_argument("-y", "--yes", action="store_true", help="skip confirmation")
    args = parser.parse_args()

    print("Vosta Engine SDK sync")
    if not check_sdk():
        return 1
    warn_if_stale()

    targets = list(args.paths)
    while True:
        if not targets:
            if args.yes:
                break
            try:
                raw = input("\nProject directory (project.json or engine/), empty to exit: ")
            except (EOFError, KeyboardInterrupt):
                print()
                break
            if not raw.strip():
                break
            targets.append(raw)

        raw = targets.pop(0)
        engine_dir = resolve_engine_dir(raw)
        if engine_dir is None:
            continue
        if engine_dir.resolve() == SDK_ROOT.resolve():
            print("[error] refusing to sync the SDK into itself.")
            continue
        sync(engine_dir, args.yes)

        if not targets and not args.yes:
            try:
                again = input("\nUpdate another project? [y/N] ").strip().lower()
            except (EOFError, KeyboardInterrupt):
                print()
                break
            if again not in ("y", "yes"):
                break

    if sys.stdin.isatty() and not args.paths:
        try:
            input("\nPress Enter to exit...")
        except (EOFError, KeyboardInterrupt):
            pass
    return 0


if __name__ == "__main__":
    sys.exit(main())
