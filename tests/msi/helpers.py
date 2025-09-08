from difflib import unified_diff
from subprocess import run, CalledProcessError
from os import path, makedirs
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
    kill_all_processes("msiexec.exe")

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


def read_config(config_file):
    if not path.exists(config_file):
        print(f"! Configuration file does not exist: {config_file}", flush=True)
        exit(1)

    with open(config_file, 'r') as file:
        content = yaml.safe_load(file)
    return content


def install(msi_file, target_folder, command_line):
    command_line = list(map(lambda x: x.replace("$MSI-FILE", msi_file), command_line))
    print(f"- Installing NSClient++: {' '.join(command_line)}", flush=True)
    try:
        return_code = run_with_timeout(command_line)
        if return_code == 0:
            print("- Installation completed successfully.", flush=True)
        else:
            print(f"! The exit code was: {return_code}", flush=True)
            exit(1)
    except Exception as e:
        print(f"! Install failed: {e}", flush=True)
        exit(1)

    if path.exists(target_folder) and path.isdir(target_folder) and path.exists(path.join(target_folder, "nscp.exe")):
        print(f"- Installation seems successfully: {target_folder}", flush=True)
    else:
        print(f"! Installation folder does not exist: {target_folder}", flush=True)
        exit(1)


def compare_file(target_folder, file_name, test_case):
    """Compare a file in the target folder with the expected content from the test case."""
    replace_password = test_case.get("replace_password", True)
    config_file = path.join(target_folder, file_name)
    if not path.exists(config_file):
        print(f"! {file_name} does not exist in the installation folder:", flush=True)
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
        print(f"- {file_name} matches expected configuration.", flush=True)
        return True
    print(f"! {file_name} does not match expected configuration:", flush=True)
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
    """Read a file and remove the UTF-8 BOM if it exists."""
    with open(file_path, 'rb') as f:
        content = f.read()
    # UTF-8 BOM
    bom = b'\xef\xbb\xbf'
    if content.startswith(bom):
        return content[len(bom):].decode('utf-8')
    return content.decode('utf-8')


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
