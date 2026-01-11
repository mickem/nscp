#!/usr/bin/env python
import os
from zipfile import ZipFile
from optparse import OptionParser
from shutil import copyfile

WANTED_PACKAGES = [
    'site-packages/requests',
    'site-packages/google',
    'site-packages/google/protobuf',
    'site-packages/google/protobuf/compiler',
    'site-packages/google/protobuf/internal',
    'site-packages/google/protobuf/pyext',
    'site-packages/google/protobuf/util',
    'encodings',
    'site-packages/jinja2',
    'site-packages/markupsafe',
]

def zipdir(path, ziph):
    missing = list(WANTED_PACKAGES)
    for root, dirs, files in os.walk(path):
        if 'pip' in root or 'Doc' in root or 'tcl' in root or 'tools' in root:
                continue
        rel_path = os.path.relpath(root, path).replace('\\', '/')
        for package in missing:
            if rel_path.startswith(package):
                missing.remove(package)
                folder = os.path.relpath(root, os.path.join(path, 'site-packages') if rel_path.startswith('site-packages') else path)
                print(f"Adding: {folder} to zip dist")
                for file in files:
                    if file.endswith('.py'):
                        ziph.write(os.path.join(root, file), os.path.join(folder, file))
    if missing:
        print(f"Error: Missing packages in {path}: {missing}")
        exit(1)

if __name__ == '__main__':

    parser = OptionParser()
    parser.add_option("-s", "--source", help="SOURCE folder of your python installation", metavar="SOURCE")
    parser.add_option("-t", "--target", help="TARGET folder to write zips in", metavar="TARGET")
    (options, args) = parser.parse_args()
    
    if not options.target:
        options.target = os.getcwd()
    if not os.path.isdir(options.target):
        os.makedirs(options.target)
    target_zip = os.path.join(options.target, 'python311.zip')

    if not options.source:
        print("Please specify source folder")
        exit(2)
    source_pyd = os.path.join(options.source, 'DLLs')
    source_lib = os.path.join(options.source, 'lib')

    with ZipFile(target_zip, 'w') as zipf:
        zipdir(source_lib, zipf)
        zipf.writestr('google/__init__.py', '')

    print("Created python lib zip: %s"%target_zip)

    for f in ['_socket.pyd', 'unicodedata.pyd']:
        copyfile(os.path.join(source_pyd, f), os.path.join(options.target, f))
