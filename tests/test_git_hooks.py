"""Run installed hooks against disposable repositories and a local bare remote."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class GitHooksTest(unittest.TestCase):
    def test_real_commit_push_and_worktree_isolation(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            env = {k: v for k, v in os.environ.items() if not k.startswith('GIT_')}
            env.update(GIT_CONFIG_GLOBAL=os.devnull, GIT_CONFIG_NOSYSTEM='1')
            repo = root / 'repo'

            def git(*args, cwd=repo, ok=True):
                result = subprocess.run(['git', *args], cwd=cwd, env=env,
                                        text=True, capture_output=True)
                if ok:
                    self.assertEqual(result.returncode, 0, result.stderr)
                else:
                    self.assertNotEqual(result.returncode, 0, result.stdout)
                return result

            git('init', '-q', '-b', 'main', str(repo), cwd=root)
            git('config', 'user.name', 'Harness Test')
            git('config', 'user.email', 'harness@example.invalid')
            for name in ['.githooks/pre-commit', '.githooks/pre-push', 'scripts/bootstrap.py',
                         'scripts/git_guard.py', 'scripts/check.py', 'scripts/run-python.sh']:
                destination = repo / name
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(ROOT / name, destination)
            # A real CLI check runs in the fixture, without recursively running this test.
            (repo / 'tests').mkdir()
            (repo / 'tests/test_fixture.py').write_text(
                'import subprocess, unittest\n'
                'class CleanHeadTest(unittest.TestCase):\n'
                '    def test_checked_head_is_clean(self):\n'
                '        self.assertEqual(subprocess.check_output(["git", "status", "--porcelain"]), b"")\n')
            (repo / '.gitignore').write_text('__pycache__/\n')
            git('add', '.')
            git('update-index', '--chmod=+x', '.githooks/pre-commit', '.githooks/pre-push')
            git('commit', '-qm', 'Fixture baseline')
            remote = root / 'remote.git'
            git('init', '--bare', '-q', str(remote), cwd=root)
            git('remote', 'add', 'origin', str(remote))
            worktree = root / 'worktree'
            git('worktree', 'add', '-b', 'codex/12-test', str(worktree))
            subprocess.run([sys.executable, 'scripts/bootstrap.py'], cwd=worktree, env=env,
                           check=True, capture_output=True)
            self.assertEqual(git('config', '--get', 'core.hooksPath', ok=False).returncode, 1)
            self.assertEqual(git('config', '--get', 'core.hooksPath', cwd=worktree).stdout.strip(), '.githooks')
            (worktree / 'change.txt').write_text('one\n')
            git('add', 'change.txt', cwd=worktree)
            git('commit', '-qm', 'Issue change', cwd=worktree)
            git('push', 'origin', 'HEAD:refs/heads/codex/12-test', cwd=worktree)
            self.assertIn('Direct push', git('push', 'origin', 'HEAD:refs/heads/main',
                                            cwd=worktree, ok=False).stderr)
            self.assertIn('same-name', git('push', 'origin', 'HEAD:refs/heads/codex/13-other',
                                          cwd=worktree, ok=False).stderr)
            (worktree / 'change.txt').write_text('two\n')
            git('add', 'change.txt', cwd=worktree)
            git('commit', '-qm', 'Second change', cwd=worktree)
            (worktree / 'change.txt').write_text('uncommitted\n')
            self.assertIn('dirty', git('push', 'origin', 'HEAD:refs/heads/codex/12-test',
                                      cwd=worktree, ok=False).stderr)
            # Install hooks in main separately; direct main commits must fail.
            subprocess.run([sys.executable, 'scripts/bootstrap.py'], cwd=repo, env=env,
                           check=True, capture_output=True)
            self.assertIn('Issue branch', git('commit', '--allow-empty', '-m', 'Forbidden', ok=False).stderr)
