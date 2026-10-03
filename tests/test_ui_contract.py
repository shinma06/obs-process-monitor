"""Static locale contracts; these do not claim an OBS GUI or live-data pass."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]


def load_locale(name):
    content = (ROOT / "data" / "locale" / f"{name}.ini").read_text(encoding="utf-8")
    entries = re.findall(r'^([A-Za-z][A-Za-z0-9.]+)="(.*)"$', content, re.MULTILINE)
    if len(entries) != len(content.splitlines()):
        raise AssertionError(f"{name}: invalid locale line")
    if len(dict(entries)) != len(entries):
        raise AssertionError(f"{name}: duplicate locale key")
    return dict(entries)


class UiLocaleContract(unittest.TestCase):
    def test_translation_keys_and_placeholders(self):
        english = load_locale("en-US")
        japanese = load_locale("ja-JP")
        self.assertEqual(set(english), set(japanese))
        for key in english:
            self.assertTrue(english[key] and japanese[key], key)
            self.assertEqual(re.findall(r"%\d+", english[key]), re.findall(r"%\d+", japanese[key]), key)

        source = (ROOT / "ProcessMonitorWidget.cpp").read_text(encoding="utf-8")
        used = set(re.findall(r'"(ProcessMonitor\.[A-Za-z]+)"', source))
        self.assertFalse(used - set(english), f"missing translations: {used - set(english)}")


if __name__ == "__main__":
    unittest.main()