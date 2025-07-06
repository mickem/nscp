from os import path
from glob import glob

from helpers import ensure_uninstalled, read_config, install, compare_file

msi_search_path = path.join("installers", "installer-NSCP", "*.msi")
msi_files = glob(msi_search_path)
if not msi_files:
    print(f"! No MSI files found in {msi_search_path}")
    exit(1)
msi_file = msi_files[0]
print(f"* Using MSI file: {msi_file}")

target_folder = path.join('c:', 'Program Files (x86)' if 'Win32' in msi_file else 'Program Files', 'NSClient++')
print(f"* Using Target folder: {target_folder}")

test_cases = [
    "normal-install.yaml",
]

for test_case_file in test_cases:
    print("Testing " + test_case_file)
    test_case_path = path.join(path.dirname(__file__), test_case_file)
    if not path.exists(test_case_path):
        print(f"! Test case file does not exist: {test_case_path}")
        exit(1)
    test_case = read_config(path.join(path.dirname(__file__), test_case_path))
    ensure_uninstalled(msi_file, target_folder)

    install(msi_file, target_folder, test_case["command_line"])

    if not compare_file(target_folder, "boot.ini", test_case):
        print("! Test failed.")
        exit(1)
    if not compare_file(target_folder, "nsclient.ini", test_case):
        print("! Test failed.")
        exit(1)

    ensure_uninstalled(msi_file, target_folder)
