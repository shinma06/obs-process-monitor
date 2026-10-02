#!/usr/bin/env python3
"""Report executable presence only. Do not read credentials or contact providers."""
import json
from pathlib import Path
import shutil
import subprocess
import platform
import sys


def main():
    tools = {name: shutil.which(name) is not None for name in
             ['git', 'gh', 'python', 'python3', 'py', 'cmake', 'node', 'npx', 'codex', 'claude', 'agent']}
    result = subprocess.run(['git', 'config', '--get', 'core.hooksPath'], capture_output=True, text=True)
    print(json.dumps({
        'executables_present': tools,
        'runtime': {'python': platform.python_version(), 'platform': sys.platform},
        'obs_build_inputs': {name: Path(name).is_file() for name in
                             ['CMakePresets.json', 'cmake/common/bootstrap.cmake', 'buildspec.json']},
        'repository_entrypoints': {name: Path(name).exists() for name in
                                  ['AGENTS.md', 'CLAUDE.md', '.agents/skills', '.claude/skills', '.cursor/rules']},
        'harness_hooks_selected': result.returncode == 0 and result.stdout.strip() == '.githooks',
        'not_checked': ['authentication', 'MCP connectivity', 'plugin trust', 'OS permissions',
                        'server rulesets', 'scheduler execution', 'remote hosts'],
    }, indent=2))


if __name__ == '__main__':
    main()
