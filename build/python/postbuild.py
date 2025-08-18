import sys
import os
import zipfile
import fnmatch
import shutil

from config import BUILD_PYTHON_FOLDER, NSCP_VERSION_NUMBER, VERSION_ARCH, BUILD_TARGET_EXE_PATH, \
    DOCS_FOLDER, ARCHIVE_FOLDER, RELEASE_SUFFIX

sys.path.append(BUILD_PYTHON_FOLDER)

def rename_and_move(file, target):
    tfile = '%s/%s'%(target, os.path.basename(file))
    tfile = tfile.replace('win64', 'x64')
    print("Copying %s to %s"%(file, tfile))
    shutil.copy(file, tfile)

def find_by_pattern(path, pattern):
    matches = []
    for root, dirnames, filenames in os.walk(path):
        if '_CPack_Packages' not in root:
            for filename in fnmatch.filter(filenames, pattern):
                if filename == "vc110.pdb":
                    continue
                matches.append(os.path.join(root, filename))
    return matches

target_docs = f'NSCP-{NSCP_VERSION_NUMBER}-{VERSION_ARCH}{RELEASE_SUFFIX}-docs.zip'
print("Gathering docs into %s"%target_docs)
docs_folder_root = os.path.join(BUILD_TARGET_EXE_PATH, DOCS_FOLDER)
matches = find_by_pattern(docs_folder_root, '*.*')
zip = zipfile.ZipFile(target_docs, 'w', zipfile.ZIP_DEFLATED)
for f in matches:
    name = os.path.relpath(f, docs_folder_root)
    zip.write(f, name)
zip.close()

if ARCHIVE_FOLDER != "":
    print("Archiving files...")
    target_installer = "%s/%s"%(ARCHIVE_FOLDER,"installers")
    target_archives = "%s/%s"%(ARCHIVE_FOLDER,"archive")
    if not os.path.isdir(ARCHIVE_FOLDER):
        os.mkdir(ARCHIVE_FOLDER)
    if not os.path.isdir(target_installer):
        os.mkdir(target_installer)
    if not os.path.isdir(target_archives):
        os.mkdir(target_archives)

    for f in find_by_pattern(BUILD_TARGET_EXE_PATH, '*%s*.msi'%NSCP_VERSION_NUMBER):
        rename_and_move(f, target_installer)
    for f in find_by_pattern(BUILD_TARGET_EXE_PATH, '*%s*.zip'%NSCP_VERSION_NUMBER):
        rename_and_move(f, target_archives)

