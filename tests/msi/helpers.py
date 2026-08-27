from difflib import unified_diff
from subprocess import run, CalledProcessError, CREATE_NEW_PROCESS_GROUP
from os import path, makedirs, environ, walk
from shutil import rmtree
from configparser import ConfigParser
import yaml
from winreg import HKEY_LOCAL_MACHINE, OpenKey, DeleteKey, KEY_ALL_ACCESS, EnumKey


def run_with_timeout(command):
    """
    Executes a command using subprocess, preventing hangs by handling stdout and stderr and timeout.
    """
    print(f" .. Executing: {' '.join(command)}", flush=True)
    try:
        result = run(
            command,
            shell=True,
            check=True,
            capture_output=True,
            creationflags=CREATE_NEW_PROCESS_GROUP,
            text=True,
            encoding='utf-8',
            errors='ignore',
            timeout=120
        )
        if result.stdout:
            print(f" .. STDOUT: {result.stdout.strip()}", flush=True)
        if result.stderr:
            print(f" .. STDERR: {result.stderr.strip()}", flush=True)

        return result.returncode

    except CalledProcessError as e:
        print(f"! Command failed with exit code {e.returncode}", flush=True)
        if e.stdout:
            print(f"! STDOUT: {e.stdout.strip()}", flush=True)
        if e.stderr:
            print(f"! STDERR: {e.stderr.strip()}", flush=True)
        if e.returncode == 1605:
            return e.returncode
        raise


def delete_registry_tree(root, subkey):
    try:
        with OpenKey(root, subkey, 0, KEY_ALL_ACCESS) as key:
            # Delete all subkeys
            i = 0
            while True:
                try:
                    sub = EnumKey(key, i)
                    delete_registry_tree(root, f"{subkey}\\{sub}")
                except OSError:
                    break
                i += 1
        DeleteKey(root, subkey)
        print(f"- Registry key deleted: {subkey}", flush=True)
    except FileNotFoundError:
        print(f"- Registry key not found, skipping deletion: {subkey}", flush=True)
    except OSError as e:
        if e.errno == 13:
            print(f"! Access denied to delete registry key {subkey}: {e}, {e.errno}", flush=True)
            return
        print(f"! Failed to delete registry key {subkey}: {e}, {e.errno}", flush=True)
        raise e



def kill_all_processes(exe_file):
    print(f"- Killing any running {exe_file} processes.", flush=True)
    try:
        run_with_timeout(["taskkill", "/F", "/IM", exe_file])
    except Exception as e:
        print(f" .. Ignoring failed to kill {exe_file}: {e}", flush=True)


def ensure_uninstalled(msi_file, target_folder):
    print(f"- Uninstalling", flush=True)
    try:
        return_code = run_with_timeout(["msiexec", "/l*", "uninstall.log", "/x", f"{msi_file}", "/q"])
    except Exception as e:
        print(f"! Uninstall process failed: {e}", flush=True)
        exit(1)
    if return_code == 1605:
        print("- No installation found, continuing with install.", flush=True)
    elif return_code == 0:
        print("- Uninstallation completed successfully.", flush=True)
    else:
        print(f"! Uninstall returned with code: {return_code}", flush=True)
        exit(1)

    kill_all_processes("nscp.exe")

    print("- Removing registry keys.", flush=True)
    delete_registry_tree(HKEY_LOCAL_MACHINE, r"Software\NSClient++")

    if path.exists(target_folder):
        print(f"- Removing folder: {target_folder}", flush=True)
        rmtree(target_folder, ignore_errors=True)

    # The modern layout keeps the writable state outside the install folder, so
    # the MSI does not own it and uninstalling leaves it behind. Left in place it
    # becomes the *destination* of the next test's migration, which then finds it
    # occupied and - correctly - refuses to merge into it, so that test fails for
    # a reason that has nothing to do with what it is testing.
    shared_folder = path.join(environ.get("ProgramData", r"c:\ProgramData"), "NSClient++")
    if path.exists(shared_folder):
        print(f"- Removing folder: {shared_folder}", flush=True)
        rmtree(shared_folder, ignore_errors=True)
        if path.exists(shared_folder):
            # It is restricted to SYSTEM and administrators; say so plainly
            # rather than letting the next test fail somewhere confusing.
            print(f"! Could not remove {shared_folder}; the next test may fail against its leftovers.", flush=True)


def generate_certificates(folder):
    """Generate a throwaway CA + server certificate pair into `folder`.

    Used by the own-certificates case (GitHub #568): the point of the
    CERTIFICATE/CERTIFICATE_KEY/CERTIFICATE_CA properties is that the agent
    serves the operator's CA-signed material instead of its generated
    self-signed fallback, so the test needs real, loadable PEM files whose
    bytes it can later find - unmodified - inside the install. Generated
    fresh on every run (never committed) for the same reason the other test
    suites generate theirs.
    """
    from datetime import datetime, timedelta, timezone
    from cryptography import x509
    from cryptography.x509.oid import NameOID
    from cryptography.hazmat.primitives import hashes, serialization
    from cryptography.hazmat.primitives.asymmetric import rsa

    makedirs(folder, exist_ok=True)

    def name(cn):
        return x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, cn)])

    def builder(subject, issuer, public_key):
        now = datetime.now(timezone.utc)
        return (x509.CertificateBuilder()
                .subject_name(name(subject))
                .issuer_name(name(issuer))
                .public_key(public_key)
                .serial_number(x509.random_serial_number())
                .not_valid_before(now - timedelta(days=1))
                .not_valid_after(now + timedelta(days=365)))

    def write(file_name, data):
        with open(path.join(folder, file_name), 'wb') as f:
            f.write(data)

    ca_key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
    ca_cert = (builder("nscp-msi-test CA", "nscp-msi-test CA", ca_key.public_key())
               .add_extension(x509.BasicConstraints(ca=True, path_length=None), critical=True)
               .sign(ca_key, hashes.SHA256()))

    server_key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
    server_cert = (builder("localhost", "nscp-msi-test CA", server_key.public_key())
                   .add_extension(x509.SubjectAlternativeName([x509.DNSName("localhost")]), critical=False)
                   .sign(ca_key, hashes.SHA256()))

    write("ca.pem", ca_cert.public_bytes(serialization.Encoding.PEM))
    write("server.pem", server_cert.public_bytes(serialization.Encoding.PEM))
    write("server.key", server_key.private_bytes(serialization.Encoding.PEM,
                                                 serialization.PrivateFormat.PKCS8,
                                                 serialization.NoEncryption()))
    print(f"- Generated test certificates in: {folder}", flush=True)


def validate_copied_files(target_folder, source_folder, copied_files):
    """Assert files the installer copied from `source_folder` byte for byte.

    `copied_files` maps install-folder-relative paths to source file names.
    Presence alone (validate_files) cannot tell an installed operator
    certificate from the self-signed one the service generates at the same
    path when it is missing - identical bytes can.
    """
    ok = True
    for installed, source in copied_files.items():
        installed_path = path.join(target_folder, installed.replace('/', path.sep))
        source_path = path.join(source_folder, source)
        if not path.exists(installed_path):
            print(f"! Installed file does not exist: {installed_path}", flush=True)
            ok = False
            continue
        with open(installed_path, 'rb') as f:
            installed_content = f.read()
        with open(source_path, 'rb') as f:
            source_content = f.read()
        if installed_content != source_content:
            print(f"! {installed_path} does not match {source_path} (the installer should copy it verbatim)", flush=True)
            ok = False
        else:
            print(f"- {installed_path} matches {source_path}", flush=True)
    return ok


def read_config(config_file):
    if not path.exists(config_file):
        print(f"! Configuration file does not exist: {config_file}", flush=True)
        exit(1)

    with open(config_file, 'r') as file:
        content = yaml.safe_load(file)
    return content


def print_install_log():
    """Print the contents of the installer log file if it exists."""
    log_file = "log.txt"
    if path.exists(log_file):
        print(f"--- Start of {log_file} ---", flush=True)
        content = read_and_remove_bom(log_file)
        safe_content = content.encode('ascii', errors='replace').decode('ascii')
        safe_content = '\n'.join(line for line in safe_content.splitlines() if line.strip())
        print(safe_content, flush=True)
        print(f"--- End of {log_file} ---", flush=True)
    else:
        print(f"! Log file {log_file} does not exist.", flush=True)


def install(msi_file, target_folder, command_line, test_data_folder=None):
    command_line = list(map(lambda x: x.replace("$MSI-FILE", msi_file), command_line))
    if test_data_folder:
        command_line = list(map(lambda x: x.replace("$TEST-DATA", test_data_folder), command_line))
    print(f"- Installing NSClient++: {' '.join(command_line)}", flush=True)
    try:
        return_code = run_with_timeout(command_line)
        if return_code == 0:
            print("- Installation completed successfully.", flush=True)
        else:
            print(f"! The exit code was: {return_code}", flush=True)
            print_install_log()
            exit(1)
    except Exception as e:
        print(f"! Install failed: {e}", flush=True)
        print_install_log()
        exit(1)

    if path.exists(target_folder) and path.isdir(target_folder) and path.exists(path.join(target_folder, "nscp.exe")):
        print(f"- Installation seems successfully: {target_folder}", flush=True)
    else:
        print(f"! Installation folder does not exist: {target_folder}", flush=True)
        print_install_log()
        exit(1)


def compare_file(target_folder, file_name, test_case):
    """Compare a file in the target folder with the expected content from the test case."""
    replace_password = test_case.get("replace_password", True)
    config_file = path.join(target_folder, file_name)
    if not path.exists(config_file):
        print(f"! {file_name} does not exist: {config_file}", flush=True)
        return False
    actual = reorder_config(read_and_remove_bom(config_file))
    # Replace any line starting with 'password =' with 'password = $$PASSWORD$$'
    if replace_password:
        actual = '\n'.join([
            'password = $$PASSWORD$$' if line.startswith('password =') else line
            for line in actual.splitlines()
        ])
    expected = reorder_config('\n'.join(test_case[file_name].splitlines()))

    if expected == actual:
        print(f"- {config_file} matches expected configuration.", flush=True)
        return True
    print(f"! {config_file} does not match expected configuration:", flush=True)
    print(f"! Differences:", flush=True)
    for line in compare_config(expected, actual):
        print(line, flush=True)
    return False


def compare_config(expected, actual):
    """Compare two configuration strings and return a list of differences."""
    expected_lines = expected.splitlines()
    actual_lines = actual.splitlines()
    if len(expected_lines) > 1 or len(actual_lines) > 1:
        diff = list(unified_diff(expected_lines, actual_lines, fromfile="expected", tofile="actual", lineterm=""))
        return diff
    return None


def reorder_config(config):
    """Reorder the configuration sections and options."""
    config_parser = ConfigParser()
    config_parser.read_string(config)
    ordered_config = []
    for section in sorted(config_parser.sections()):
        ordered_config.append(f"[{section}]")
        for option in sorted(config_parser.options(section)):
            value = config_parser.get(section, option)
            ordered_config.append(f"{option} = {value}")
        ordered_config.append("")
    return "\n".join(ordered_config).strip()


def read_and_remove_bom(file_path):
    """Read a file and remove the BOM if it exists (supports UTF-8, UTF-16 LE, and UTF-16 BE)."""
    with open(file_path, 'rb') as f:
        content = f.read()
    try:
        # UTF-16 LE BOM
        if content.startswith(b'\xff\xfe'):
            return content[2:].decode('utf-16-le')
        # UTF-16 BE BOM
        if content.startswith(b'\xfe\xff'):
            return content[2:].decode('utf-16-be')
        # UTF-8 BOM
        if content.startswith(b'\xef\xbb\xbf'):
            return content[3:].decode('utf-8')
        return content.decode('utf-8')
    except UnicodeDecodeError as e:
        print(f"! Failed to decode file {file_path}: {e}", flush=True)
        return content.decode('utf-8', errors='replace')


def create_upgrade_config(upgrade_config, target_folder):
    """Create folders and config files to simulate upgrade."""
    if not path.exists(target_folder):
        print(f"- Creating target folder: {target_folder}", flush=True)
        makedirs(target_folder, exist_ok=True)
    if 'boot.ini' in upgrade_config:
        boot_ini_path = path.join(target_folder, "boot.ini")
        print(f"- Creating boot.ini file: {boot_ini_path}", flush=True)
        with open(boot_ini_path, 'w') as file:
            file.write(upgrade_config['boot.ini'])
        print("- boot.ini file created successfully.", flush=True)
    if 'nsclient.ini' in upgrade_config:
        nsclient_ini_path = path.join(target_folder, "nsclient.ini")
        print(f"- Creating nsclient.ini file: {nsclient_ini_path}", flush=True)
        with open(nsclient_ini_path, 'w') as file:
            file.write(upgrade_config['nsclient.ini'])
        print("- nsclient.ini file created successfully.", flush=True)
    # Anything else an existing installation would have on disk, keyed by path
    # relative to the install folder - e.g. an enrolled host's fleet\fleet.ini.
    # %VAR% is expanded and an absolute result wins over the install folder, so
    # a case can seed the modern layout's %ProgramData%\NSClient++ as well.
    for relative, content in upgrade_config.get('files', {}).items():
        file_path = path.join(target_folder, path.expandvars(relative))
        makedirs(path.dirname(file_path), exist_ok=True)
        print(f"- Creating file: {file_path}", flush=True)
        with open(file_path, 'w') as file:
            file.write(content)


def validate_files(target_folder, required_files):
    """Validate that required files exist in the target folder."""
    all_exist = True
    for file_group in required_files.keys():
        missing_files = []
        print(f"- Validating required files in block: {file_group} (in {target_folder})", flush=True)
        for req_file in required_files[file_group]:
            file_path = path.join(target_folder, req_file.replace('/', path.sep))
            if not path.exists(file_path):
                missing_files.append(file_path)
                all_exist = False
        if missing_files:
            missing_files_str = ", ".join(missing_files)
            print(f"! Required file in {file_group} does not exist: {missing_files_str}", flush=True)
    return all_exist


def validate_files_absent(target_folder, forbidden_files):
    """Validate that files which a deselected feature owns are NOT installed.

    The mirror of validate_files: a feature that can be turned off is only
    really optional if leaving it out actually leaves its files out, and that
    is exactly what a stray ComponentRef in another feature breaks without
    anything else failing.
    """
    none_exist = True
    for file_group in forbidden_files.keys():
        present_files = []
        print(f"- Validating absent files in block: {file_group} (in {target_folder})", flush=True)
        for bad_file in forbidden_files[file_group]:
            file_path = path.join(target_folder, bad_file.replace('/', path.sep))
            if path.exists(file_path):
                present_files.append(file_path)
                none_exist = False
        if present_files:
            present_files_str = ", ".join(present_files)
            print(f"! File in {file_group} should not have been installed: {present_files_str}", flush=True)
    return none_exist

def resolve_folder(target_folder, folder):
    """Resolve a folder reference from a test case.

    `None` means the install folder. Anything else is taken literally, with
    %ProgramData% expanded, so a test case can point at the modern layout's
    shared folder without hardcoding a drive letter.
    """
    if not folder:
        return target_folder
    return path.expandvars(folder)


def validate_secured(folder):
    """Assert that `folder` is readable only by SYSTEM and administrators.

    The modern layout moves the configuration (which holds passwords) and the
    fleet identity's private key into %ProgramData%, which grants
    `Users: Read & Execute` by inheritance. Breaking that inheritance is the
    whole point of the move, and it is invisible when it fails: the files are
    all in the right place and every check passes, they are just readable by
    every account on the machine.

    So this asserts the outcome (no ACE for anyone else) rather than that the
    installer tried - see docs/design/shared-folder-migration.md.
    """
    if not path.exists(folder):
        print(f"! Cannot check permissions, folder does not exist: {folder}", flush=True)
        return False

    # Every entry, not just the folder: a migrated file arrives by rename and
    # keeps the security descriptor it had at the source, so the folder can be
    # perfectly restricted around world-readable secrets. Checking one path at
    # a time keeps the icacls output unambiguous (paths can contain spaces).
    ok = True
    items = [folder]
    for root, dirs, files in walk(folder):
        items.extend(path.join(root, name) for name in dirs + files)
    for item in items:
        result = run(['icacls', item], capture_output=True, text=True)
        if result.returncode != 0:
            print(f"! icacls failed for {item}: {result.stderr.strip()}", flush=True)
            ok = False
            continue

        allowed = (r'NT AUTHORITY\SYSTEM', r'BUILTIN\Administrators')
        offenders = []
        for line in result.stdout.splitlines():
            # "<path> PRINCIPAL:(perms)" on the first line, then "  PRINCIPAL:(perms)".
            entry = line.replace(item, '', 1).strip()
            if not entry or ':' not in entry or entry.startswith('Successfully processed'):
                continue
            principal = entry.rsplit(':', 1)[0].strip()
            if principal not in allowed:
                offenders.append(principal)

        if offenders:
            print(f"! {item} grants access to: {', '.join(sorted(set(offenders)))}", flush=True)
            print(f"! Only {' and '.join(allowed)} may have access.", flush=True)
            print(result.stdout, flush=True)
            ok = False
    if ok:
        print(f"- {folder} and its contents are restricted to SYSTEM and administrators.", flush=True)
    return ok
