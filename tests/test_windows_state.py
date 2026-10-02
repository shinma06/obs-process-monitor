"""Exercise native ACL and junction boundaries without administrator privileges."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
if os.name == 'nt':
    import windows_state
    import handoff_registry


@unittest.skipUnless(os.name == 'nt', 'Native Windows ACL tests')
class WindowsStateTest(unittest.TestCase):
    def test_rejects_junction_and_unsafe_directory_without_repair(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            outside = root / 'outside'
            outside.mkdir()
            alias = root / 'alias'
            subprocess.run(['cmd.exe', '/d', '/c', 'mklink', '/J', str(alias), str(outside)],
                           check=True, capture_output=True)
            try:
                with self.assertRaises(ValueError):
                    windows_state.private_path(alias, create=True)
                self.assertEqual(list(outside.iterdir()), [])
            finally:
                alias.rmdir()  # Only the junction, never its target.
            private = windows_state.private_path(root / 'private', create=True)
            subprocess.run(['icacls', str(private), '/grant', '*S-1-1-0:(OI)(CI)R'],
                           check=True, capture_output=True)
            before = subprocess.check_output(['icacls', str(private)])
            with self.assertRaises(ValueError):
                handoff_registry.register(private, {'owner': 'test'}, str(root), 'host')
            self.assertEqual(before, subprocess.check_output(['icacls', str(private)]))
            self.assertEqual(list(private.iterdir()), [])

    def test_lock_serializes_processes(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory = Path(tmp) / 'lease'
            code = ('import sys; from pathlib import Path; import windows_state; '
                    'gate = windows_state.locked(Path(sys.argv[1])); gate.__enter__(); '
                    'print("locked", flush=True); sys.stdin.readline(); gate.__exit__(None,None,None)')
            env = dict(os.environ, PYTHONPATH=str(Path(__file__).resolve().parents[1] / 'scripts'))
            with windows_state.locked(directory):
                child = subprocess.Popen([sys.executable, '-c', code, str(directory)], env=env,
                                         stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                         stderr=subprocess.PIPE, text=True)
                # A competing lock must not finish while this process owns it.
                with self.assertRaises(subprocess.TimeoutExpired):
                    child.wait(timeout=2)
            try:
                output, error = child.communicate('\n', timeout=20)
                self.assertEqual(child.returncode, 0, error)
                self.assertEqual(output.strip(), 'locked')
            finally:
                if child.poll() is None:
                    child.kill()
                    child.wait()
