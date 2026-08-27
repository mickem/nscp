from os import path, listdir, environ
from glob import glob
from argparse import ArgumentParser

from helpers import ensure_uninstalled, read_config, install, compare_file, create_upgrade_config, validate_files, validate_files_absent, resolve_folder, validate_secured, generate_certificates, validate_copied_files

# Argument parsing for test selection
parser = ArgumentParser(description="Run NSCP MSI installer tests.")
parser.add_argument('tests', nargs='*', help='Test case YAML files to run (default: all)')
parser.add_argument('--matches', '-m', help='Run all test cases containing the given substring')
parser.add_argument('--path', '-p', help='Path to the installer MSI files (default: ./installers/installer-NSCP)', default='installers/installer-NSCP')
parser.add_argument('--keep', help='Keep installed files after installation', action='store_true')
args = parser.parse_args()

msi_files = glob(path.join(args.path, '*.msi'))
if not msi_files:
    print(f"! No MSI files found in {args.path}", flush=True)
    exit(1)
msi_file = path.abspath(msi_files[0])
print(f"* Using MSI file: {msi_file}", flush=True)

target_folder = path.join('c:\\', 'Program Files (x86)' if 'Win32' in msi_file else 'Program Files', 'NSClient++')
print(f"* Using Target folder: {target_folder}", flush=True)

TEST_FOLDER = path.join(path.dirname(__file__), 'tests')

# Where generated per-run test data (e.g. the own-certificates case's
# throwaway CA and server pair) lives, referenced from a test case's
# command_line as $TEST-DATA. %ProgramData% rather than %TEMP% so the path has
# no spaces and is readable by both the installing user and SYSTEM (the
# deferred custom actions).
TEST_DATA_FOLDER = path.join(environ.get("ProgramData", r"c:\ProgramData"), "nscp-msi-test")

all_test_cases = [
    f for f in listdir(TEST_FOLDER)
    if f.endswith('.yaml')
]

if args.matches:
    test_cases = [f for f in all_test_cases if args.matches in f]
else:
    test_cases = args.tests if args.tests else all_test_cases

failure = False
results = []

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

    if test_case.get('certificates'):
        generate_certificates(TEST_DATA_FOLDER)

    install(msi_file, target_folder, test_case["command_line"], TEST_DATA_FOLDER)

    # boot.ini always stays beside the executable - it is what tells the agent
    # where everything else lives. The configuration follows the layout, so a
    # test case can point at another folder with `config_folder`.
    if not compare_file(target_folder, "boot.ini", test_case):
        print("! Test failed.", flush=True)
        failure = True
    config_folder = resolve_folder(target_folder, test_case.get('config_folder'))
    if not compare_file(config_folder, "nsclient.ini", test_case):
        print("! Test failed.", flush=True)
        failure = True
    if 'secured_folder' in test_case:
        if not validate_secured(resolve_folder(target_folder, test_case['secured_folder'])):
            print("! Test failed.", flush=True)
            failure = True
    if 'absent_files' in test_case:
        # Files an upgrade should have *moved*, checked where they used to be.
        if not validate_files_absent(target_folder, test_case['absent_files']):
            print("! Test failed.", flush=True)
            failure = True
    if 'required_files' in test_case:
        if not validate_files(target_folder, test_case['required_files']):
            print("! Test failed.", flush=True)
            failure = True
    if 'forbidden_files' in test_case:
        if not validate_files_absent(target_folder, test_case['forbidden_files']):
            print("! Test failed.", flush=True)
            failure = True
    if 'installed_files' in test_case:
        # Files the installer must have copied verbatim from the test data
        # folder - presence alone cannot tell them from generated ones.
        if not validate_copied_files(target_folder, TEST_DATA_FOLDER, test_case['installed_files']):
            print("! Test failed.", flush=True)
            failure = True

    if failure:
        results.append((test_case_file, False))
        break
    if not args.keep:
        ensure_uninstalled(msi_file, target_folder)
    results.append((test_case_file, True))
print("", flush=True)
print("- Test results:", flush=True)
for test_case_file, result in results:
    print(f"  {'PASS' if result else 'FAIL'} - {test_case_file}", flush=True)
if failure:
    print("! One or more tests failed.", flush=True)
    exit(1)
else:
    print("- All tests passed successfully.", flush=True)
