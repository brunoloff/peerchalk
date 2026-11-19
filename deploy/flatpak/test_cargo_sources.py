#!/usr/bin/env python3
"""Tests for cargo_sources.py: python3 deploy/flatpak/test_cargo_sources.py"""
import json
import pathlib
import subprocess
import sys
import tempfile
import unittest

SCRIPT = pathlib.Path(__file__).with_name("cargo_sources.py")
REGISTRY = 'source = "registry+https://github.com/rust-lang/crates.io-index"'


def run(lock_text):
    with tempfile.TemporaryDirectory() as folder:
        lock = pathlib.Path(folder, "Cargo.lock")
        lock.write_text(lock_text)
        return subprocess.run([sys.executable, "-I", SCRIPT, lock], capture_output=True, text=True)


class CargoSourcesTest(unittest.TestCase):
    def test_lists_registry_crates_and_skips_local_ones(self):
        result = run('[[package]]\nname = "app"\nversion = "1.0.0"\n\n'
                     f'[[package]]\nname = "itoa"\nversion = "1.0.1"\n{REGISTRY}\nchecksum = "ab12"\n')
        self.assertEqual(result.returncode, 0, result.stderr)
        sources = json.loads(result.stdout)
        self.assertEqual(len(sources), 3)
        self.assertEqual(sources[0]["url"], "https://static.crates.io/crates/itoa/itoa-1.0.1.crate")
        self.assertEqual(sources[0]["sha256"], "ab12")
        self.assertEqual(json.loads(sources[1]["contents"])["package"], "ab12")
        self.assertEqual(sources[2]["dest-filename"], "config.toml")

    def test_refuses_git_dependencies(self):
        result = run('[[package]]\nname = "x"\nversion = "0.1.0"\n'
                     'source = "git+https://example.com/x.git#abc"\n')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("only crates.io sources", result.stderr)


if __name__ == "__main__":
    unittest.main()
