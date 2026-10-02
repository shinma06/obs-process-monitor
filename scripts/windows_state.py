"""Windows private state for cooperative tools, using built-in ACLs and file locks.

No third-party module or administrator rights are required. Existing ACLs are
validated, never repaired. This is not isolation from malicious same-user code.
"""
from contextlib import contextmanager
import os
from pathlib import Path
import stat
import subprocess


def reject_links(path):
    for item in (path, *path.parents):
        try:
            info = item.lstat()
        except FileNotFoundError:
            continue
        if info.st_file_attributes & stat.FILE_ATTRIBUTE_REPARSE_POINT:
            raise ValueError('Private state must not contain reparse points')


def private_path(path, *, create=False, create_file=False):
    """Require current-user ownership and grants limited to user/SYSTEM/admins."""
    path = Path(path).absolute()
    reject_links(path)
    # Pass paths as data, never interpolate them into PowerShell source.
    script = r'''
$ErrorActionPreference = 'Stop'
$p = $env:HARNESS_ACL_PATH
$sid = [System.Security.Principal.WindowsIdentity]::GetCurrent().User
$file = $env:HARNESS_ACL_FILE -eq '1'
if ($file -or !(Test-Path -LiteralPath $p)) {
    if ($env:HARNESS_ACL_CREATE -ne '1') { throw 'Private state is missing' }
    if ($file) {
        $acl = [System.Security.AccessControl.FileSecurity]::new()
        $inherit = [System.Security.AccessControl.InheritanceFlags]::None
    } else {
        $acl = [System.Security.AccessControl.DirectorySecurity]::new()
        $inherit = [System.Security.AccessControl.InheritanceFlags]'ContainerInherit,ObjectInherit'
    }
    $acl.SetOwner($sid)
    $acl.SetAccessRuleProtection($true, $false)
    foreach ($id in @($sid.Value, 'S-1-5-18', 'S-1-5-32-544')) {
        $principal = [System.Security.Principal.SecurityIdentifier]::new($id)
        $rule = [System.Security.AccessControl.FileSystemAccessRule]::new(
            $principal, 'FullControl', $inherit, 'None', 'Allow')
        $acl.AddAccessRule($rule)
    }
    if ($file) {
        # Elevated processes otherwise default to the Administrators group owner.
        # CreateNew atomically refuses an existing file; no existing ACL is repaired.
        $stream = [System.IO.FileStream]::new($p, [System.IO.FileMode]::CreateNew,
            [System.Security.AccessControl.FileSystemRights]::FullControl,
            [System.IO.FileShare]::Read, 4096, [System.IO.FileOptions]::None, $acl)
        $stream.Dispose()
    } else {
        $null = [System.IO.Directory]::CreateDirectory($p, $acl)
    }
}
$item = Get-Item -LiteralPath $p -Force
if (($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
    throw 'Reparse points are forbidden'
}
$acl = $item.GetAccessControl()
if ($acl.GetOwner([System.Security.Principal.SecurityIdentifier]).Value -ne $sid.Value) {
    throw 'Private state is not owned by the current user'
}
$allowed = @($sid.Value, 'S-1-5-18', 'S-1-5-32-544')
$rules = $acl.GetAccessRules($true, $true, [System.Security.Principal.SecurityIdentifier])
if ($rules.Count -eq 0) { throw 'Empty access control list is unsupported' }
foreach ($rule in $rules) {
    if ($rule.AccessControlType -eq 'Allow' -and $rule.IdentityReference.Value -notin $allowed) {
        throw 'Private state grants access to another principal'
    }
}
'''
    env = dict(os.environ, HARNESS_ACL_PATH=str(path), HARNESS_ACL_CREATE='1' if create else '0',
               HARNESS_ACL_FILE='1' if create_file else '0')
    result = subprocess.run(['powershell.exe', '-NoLogo', '-NoProfile', '-NonInteractive',
                             '-Command', script], env=env, capture_output=True, timeout=30)
    if result.returncode:
        # Do not disclose private paths, user SIDs, or command diagnostics.
        raise ValueError('Windows private state ACL validation failed; explicit owner recovery required')
    reject_links(path)
    return path


@contextmanager
def record(path, mode):
    path = Path(path)
    reject_links(path)
    if mode == 'x' or not path.exists():
        if mode == 'r':
            raise FileNotFoundError(path)
        private_path(path, create=True, create_file=True)
        if mode == 'x':
            mode = 'r+'
    else:
        private_path(path)
        if path.stat().st_nlink != 1:
            raise ValueError('Private records must not have hard links')
    with path.open(mode, encoding='utf-8') as stream:
        private_path(path)
        yield stream


@contextmanager
def locked(directory):
    import msvcrt
    directory = private_path(directory, create=True)
    with record(directory / 'mutex', 'a+') as gate:
        gate.seek(0, os.SEEK_END)
        if gate.tell() == 0:
            gate.write('0')
            gate.flush()
        gate.seek(0)
        msvcrt.locking(gate.fileno(), msvcrt.LK_LOCK, 1)
        try:
            yield directory / 'lease.json'
        finally:
            gate.seek(0)
            msvcrt.locking(gate.fileno(), msvcrt.LK_UNLCK, 1)
