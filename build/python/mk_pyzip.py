#!/usr/bin/env python
import os
from zipfile import ZipFile
from optparse import OptionParser
from shutil import copyfile

def zipdir(path, ziph):
    for root, dirs, files in os.walk(path):
        dir = os.path.relpath(root, path)
        for file in files:
            if 'site-packages' in root and 'requests' not in root:
                continue
            if file.endswith('.py'):
                ziph.write(os.path.join(root, file), os.path.join(dir, file))

if __name__ == '__main__':

    parser = OptionParser()
    parser.add_option("-s", "--source", help="SOURCE folder of your python installation", metavar="SOURCE")
    parser.add_option("-t", "--target", help="TARGET folder to write zips in", metavar="TARGET")
    (options, args) = parser.parse_args()
    
    if not options.target:
        options.target = os.getcwd()
    if not os.path.isdir(options.target):
        os.makedirs(options.target)
    target_zip = os.path.join(options.target, 'python37.zip')

    if not options.source:
        print("Please specify source folder")
        exit(2)
    source_pyd = os.path.join(options.source, 'DLLs')

    with ZipFile(target_zip, 'w') as zipf:
        zipdir(options.source, zipf)
    print("Created python lib zip: %s"%target_zip)

    for f in ['_socket.pyd', 'unicodedata.pyd']:
        copyfile(os.path.join(source_pyd, f), os.path.join(options.target, f))
