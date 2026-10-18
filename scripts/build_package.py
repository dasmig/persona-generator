#!/usr/bin/env python3
"""Build the @dasmig/persona npm package into build/package.

    python3 scripts/build_package.py [--libs DIR] [--out DIR] [--em em++]

Compiles wasm/persona_api.cpp with Emscripten (3.1.74 in CI) against the
libraries persona-generator composes, which are folders named
<library>-generator under --libs (default: $LIBS, or the parent folder, as in
the Makefile), with their lite data compiled in. It then assembles the
package: the module (persona.mjs, persona.wasm), the JavaScript in package/,
the licence, DATA_LICENSES.txt gathered from the libraries, and versions.mjs,
the library versions (git tags) the module was built from, because a persona
replays only with the same data.

Test it with `node package/test.mjs build/package` and
`node package/browser-test.mjs build/package`; pack it with
`npm pack ./build/package`.
"""

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# Libraries with data compiled into the module.
DATA_LIBRARIES = ["country", "city", "birth", "biodata", "name", "interest", "occupation", "trait"]
# Every library the module is built from, for versions.mjs (persona itself first).
VERSION_LIBRARIES = ["persona", "entity"] + DATA_LIBRARIES

PACKAGE_FILES = [
    "package.json", "index.mjs", "node.mjs", "generator.mjs", "batch.mjs", "pool.mjs",
    "pool-worker.mjs", "pool-worker-node.mjs", "pool-worker-core.mjs", "index.d.ts", "README.md",
]

EXPORTED_FUNCTIONS = ["_dasmig_persona_generate", "_dasmig_persona_generate_batch", "_dasmig_persona_info"]


def git_describe(directory: Path) -> str:
    try:
        result = subprocess.run(["git", "describe", "--tags", "--always", "--dirty"], cwd=directory,
                                capture_output=True, text=True, check=True)
        return result.stdout.strip() or "unknown"
    except (OSError, subprocess.CalledProcessError):
        return "unknown"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--libs", default=os.environ.get("LIBS", str(ROOT.parent)),
                        help="folder holding the <library>-generator checkouts")
    parser.add_argument("--out", default=str(ROOT / "build" / "package"), help="folder to assemble the package in")
    parser.add_argument("--em", default="em++", help="the Emscripten C++ compiler")
    args = parser.parse_args()

    libs = Path(args.libs).resolve()
    out = Path(args.out).resolve()
    lib_dir = {name: libs / f"{name}-generator" for name in VERSION_LIBRARIES if name != "persona"}
    for name, path in lib_dir.items():
        if not path.is_dir():
            print(f"missing {path}: put the {name}-generator checkout there or pass --libs", file=sys.stderr)
            return 1
    lib_dir["persona"] = ROOT

    out.mkdir(parents=True, exist_ok=True)
    admin1 = lib_dir["city"] / "resources" / "admin1CodesASCII.txt"

    # The module: native WebAssembly exceptions (JavaScript ones route every
    # call that may throw through JavaScript, which loaded the data about 1.6
    # times slower), for browsers and workers only so bundlers find no Node
    # code in it; the package's Node entry reads the .wasm and passes it in.
    # City's admin1 names are embedded, so nothing is fetched but persona.wasm.
    command = [
        args.em, "-std=c++23", "-O3", "-DNDEBUG", "-fwasm-exceptions",
        "-I", str(ROOT / "wasm"), "-I", str(ROOT),
    ]
    command += ["-I" + str(lib_dir[name]) for name in ["entity"] + DATA_LIBRARIES]
    command += [f"-DDASMIG_{name.upper()}_EMBED" for name in DATA_LIBRARIES]
    command += [
        str(ROOT / "wasm" / "persona_api.cpp"), "-o", str(out / "persona.mjs"),
        "-sWASM=1", "-sALLOW_MEMORY_GROWTH=1", "-sMODULARIZE=1", "-sEXPORT_ES6=1",
        "-sEXPORT_NAME=createPersonaModule", "-sENVIRONMENT=web,worker",
        "-sEXPORTED_FUNCTIONS=" + ",".join(EXPORTED_FUNCTIONS), "-sEXPORTED_RUNTIME_METHODS=cwrap",
        "--embed-file", f"{admin1}@/resources/city/admin1CodesASCII.txt",
    ]
    print("+", " ".join(command), flush=True)
    subprocess.run(command, check=True)

    for name in PACKAGE_FILES:
        shutil.copyfile(ROOT / "package" / name, out / name)
    shutil.copyfile(ROOT / "LICENSE.MIT", out / "LICENSE")

    # The package ships the libraries' compiled-in data, so it carries their
    # data licences, gathered from each checkout.
    text = ("The persona module compiles in the lite data of eight dasmig libraries.\n"
            "Their data sources and licences follow, as each library states them.\n")
    for name in DATA_LIBRARIES:
        library = f"{name}-generator"
        text += f"\n== {library} ==\n\n"
        licence = lib_dir[name] / "LICENSE_DATA.txt"
        if licence.is_file():
            text += licence.read_text(encoding="utf-8")
        else:
            text += f"See https://github.com/dasmig/{library}/blob/HEAD/doc/data.md (Source).\n"
    (out / "DATA_LICENSES.txt").write_text(text, encoding="utf-8", newline="\n")

    versions = ("// Generated by scripts/build_package.py: the library versions this module was built from.\n"
                "export const versions = Object.freeze({\n")
    for name in VERSION_LIBRARIES:
        versions += f"  '{name}-generator': '{git_describe(lib_dir[name])}',\n"
    versions += "});\n"
    (out / "versions.mjs").write_text(versions, encoding="utf-8", newline="\n")

    print(f"built {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
