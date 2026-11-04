"""Generate the RecompLauncher manifest for a desktop package."""

import argparse
import json
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("platform", choices=("windows", "macos", "linux"))
    parser.add_argument("output", type=Path)
    parser.add_argument("--appimage", help="Packaged Linux AppImage filename")
    args = parser.parse_args()
    if args.platform == "linux" and not args.appimage:
        parser.error("Linux requires --appimage")

    source = Path(__file__).resolve().parents[1] / "recomplauncher.json"
    profile = json.loads(source.read_text(encoding="utf-8"))
    if args.platform != "windows":
        program = args.appimage if args.platform == "linux" else "Dusklight.app/Contents/MacOS/Dusklight"
        profile["start"]["program"] = [program]
        paths = profile["config"][0]["file"]
        paths.pop(0)
        profile["saves"] = [path for path in profile["saves"] if not path.startswith("{install}/")]
        if args.platform == "linux":
            for entry in paths:
                for key in entry:
                    entry[key] = entry[key].replace("{config}", "{local}")
            profile["saves"] = [path.replace("{config}", "{local}") for path in profile["saves"]]

    args.output.write_text(json.dumps(profile, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
