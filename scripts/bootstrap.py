#!/usr/bin/env python3
"""Install repository hooks explicitly; preserve unrelated custom hooks."""
from pathlib import Path
import os
import subprocess
import sys


def bootstrap():
    root = Path(subprocess.check_output(['git', 'rev-parse', '--show-toplevel'], text=True).strip())
    hook_dir = root / '.githooks'
    if not all((hook_dir / name).is_file() for name in ['pre-commit', 'pre-push']):
        raise ValueError('Run in a repository containing this harness.')
    scopes = [[], ['--local']]
    per_tree = subprocess.run(['git', 'config', '--bool', '--get', 'extensions.worktreeConfig'],
                              capture_output=True, text=True)
    if per_tree.returncode not in (0, 1):
        raise ValueError('Cannot inspect worktree configuration.')
    if per_tree.stdout.strip() == 'true':
        scopes.append(['--worktree'])
    for scope in scopes:
        result = subprocess.run(['git', 'config', *scope, '--get', 'core.hooksPath'],
                                capture_output=True, text=True)
        if result.returncode == 1:
            if not scope:
                default_hooks = Path(subprocess.check_output(
                    ['git', 'rev-parse', '--path-format=absolute', '--git-path', 'hooks'], text=True).strip())
                if default_hooks.exists() and any(
                        p.is_file() and not p.name.endswith('.sample') and os.access(p, os.X_OK)
                        for p in default_hooks.iterdir()):
                    raise ValueError('Active default Git hooks found; integrate manually without disabling them.')
            continue
        if result.returncode != 0:
            raise ValueError('Cannot inspect existing hooks.')
        path = Path(result.stdout.strip())
        if not path.is_absolute():
            path = root / path
        if path.resolve() != hook_dir.resolve():
            raise ValueError('Custom hooks found; integrate manually without overwriting them.')
    for name in ['pre-commit', 'pre-push']:
        p = hook_dir / name
        p.chmod(p.stat().st_mode | 0o111)
    git_dir = Path(subprocess.check_output(
        ['git', 'rev-parse', '--absolute-git-dir'], text=True).strip())
    common_dir = Path(subprocess.check_output(
        ['git', 'rev-parse', '--path-format=absolute', '--git-common-dir'], text=True).strip())
    # Linked worktrees must not select missing hooks in another checkout.
    if git_dir != common_dir and per_tree.stdout.strip() != 'true':
        for key in ('core.bare', 'core.worktree'):
            setting = subprocess.run(['git', 'config', '--local', '--get', key], capture_output=True, text=True)
            if setting.returncode not in (0, 1) or (setting.returncode == 0 and
                    (key == 'core.worktree' or setting.stdout.strip() != 'false')):
                raise ValueError('Existing core.bare/core.worktree needs manual worktreeConfig migration.')
        subprocess.run(['git', 'config', '--local', 'extensions.worktreeConfig', 'true'], check=True)
    scope = '--worktree' if git_dir != common_dir or per_tree.stdout.strip() == 'true' else '--local'
    subprocess.run(['git', 'config', scope, 'core.hooksPath', '.githooks'], check=True)
    subprocess.run(['git', 'config', scope, 'harness.python', sys.executable], check=True)
    print('Hooks installed for this repository. Server protection is configured separately.')


if __name__ == '__main__':
    try:
        bootstrap()
    except (ValueError, subprocess.CalledProcessError) as error:
        raise SystemExit(str(error))
