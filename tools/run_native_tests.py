"""Run native policy/input tests without reading local firmware credentials."""
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
compiler = shutil.which("g++")
if not compiler:
    raise SystemExit("Install g++ and add it to PATH to run native checks.")
with tempfile.TemporaryDirectory(prefix="spotify-remote-tests-") as temp:
    for source in sorted((root / "tests").glob("*_test.cpp")):
        output = Path(temp) / (source.stem + ".exe")
        subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra", "-I", str(root / "src" / "app"), str(source), "-o", str(output)], check=True)
        subprocess.run([str(output)], check=True)
print("All native checks passed.")
