from os import path, listdir
from glob import glob
from argparse import ArgumentParser

from helpers import ensure_uninstalled, read_config, install, compare_file, create_upgrade_config

msi_search_path = path.join("installers", "installer-NSCP", "*.msi")
msi_files = glob(msi_search_path)
if not msi_files:
    print(f"! No MSI files found in {msi_search_path}", flush=True)
    exit(1)
msi_file = msi_files[0]
print(f"* Using MSI file: {msi_file}", flush=True)

target_folder = path.join('c:\\', 'Program Files (x86)' if 'Win32' in msi_file else 'Program Files', 'NSClient++')
print(f"* Using Target folder: {target_folder}", flush=True)

# Argument parsing for test selection
parser = ArgumentParser(description="Run NSCP MSI installer tests.")
parser.add_argument('tests', nargs='*', help='Test case YAML files to run (default: all)')
parser.add_argument('--matches', '-m', help='Run all test cases containing the given substring')
args = parser.parse_args()

TEST_FOLDER = path.join(path.dirname(__file__), 'tests')

all_test_cases = [
    f for f in listdir(TEST_FOLDER)
    if f.endswith('.yaml')
]

if args.matches:
    test_cases = [f for f in all_test_cases if args.matches in f]
else:
    test_cases = args.tests if args.tests else all_test_cases

for test_case_file in test_cases:
    print("", flush=True)
    print("Testing " + test_case_file, flush=True)
    test_case_path = path.join(TEST_FOLDER, test_case_file)
    if not path.exists(test_case_path):
        print(f"! Test case file does not exist: {test_case_path}", flush=True)
        exit(1)
    test_case = read_config(path.join(TEST_FOLDER, test_case_path))
    ensure_uninstalled(msi_file, target_folder)

    if 'upgrade' in test_case:
        create_upgrade_config(test_case['upgrade'], target_folder)

    install(msi_file, target_folder, test_case["command_line"])

    failure = False

    if not compare_file(target_folder, "boot.ini", test_case):
        print("! Test failed.", flush=True)
        failure = True
    if not compare_file(target_folder, "nsclient.ini", test_case):
        print("! Test failed.", flush=True)
        failure = True

    ensure_uninstalled(msi_file, target_folder)
    if failure:
        print("! One or more tests failed.", flush=True)
        exit(1)
    else:
        print("- All tests passed successfully.", flush=True)
