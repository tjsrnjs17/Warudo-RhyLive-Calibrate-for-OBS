import argparse
import hashlib
from pathlib import Path
import re
import shutil
import zipfile


def package(dll, output):
    root = Path(__file__).resolve().parent.parent
    version = re.search(r"project\(warudo-calibrate VERSION ([0-9.]+)",
                        (root / "CMakeLists.txt").read_text(encoding="utf-8")).group(1)
    output.mkdir(parents=True, exist_ok=True)
    names = ["CMakeLists.txt", "README.md", "README.ko.md", "LICENSE", "NOTICE", "CHANGELOG.md",
             ".gitignore", ".gitattributes"]
    sources = [root / name for name in names]
    for folder in ["src", "locale", "scripts", "tests", "docs", ".github"]:
        sources.extend(path for path in sorted((root / folder).rglob("*"))
                       if path.is_file() and "__pycache__" not in path.parts and path.suffix != ".pyc")
    prefix = f"warudo-calibrate-v{version}"
    source_zip = output / f"{prefix}-source.zip"
    with zipfile.ZipFile(source_zip, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for path in sources:
            archive.write(path, f"{prefix}/{path.relative_to(root).as_posix()}")
    binary_zip = output / f"{prefix}-windows-x64.zip"
    with zipfile.ZipFile(binary_zip, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        archive.write(dll, "obs-plugins/64bit/warudo-calibrate.dll")
        for name in ["README.md", "README.ko.md", "LICENSE", "NOTICE", "CHANGELOG.md"]:
            archive.write(root / name, name)
        for path in sorted((root / "docs").rglob("*")):
            if path.is_file():
                archive.write(path, path.relative_to(root).as_posix())
    destination = output / "warudo-calibrate.dll"
    if dll.resolve() != destination.resolve():
        shutil.copy2(dll, destination)
    paths = [source_zip, binary_zip, destination]
    lines = [hashlib.sha256(path.read_bytes()).hexdigest() + "  " + path.name for path in paths]
    (output / "SHA256SUMS.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")
    for path in paths:
        print(f"{path.name}: {path.stat().st_size} bytes")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("dll", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    if not args.dll.is_file():
        parser.error("DLL does not exist")
    package(args.dll, args.output)
