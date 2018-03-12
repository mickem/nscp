#!/usr/bin/env python
import os
import zipfile
import json
import argparse

def read_json(file):
    return json.load(open(file))

def zipdir(path, ziph):
    for root, dirs, files in os.walk(path):
        for file in files:
            if not file == "CMakeLists.txt":
                f = os.path.join(root, file)
                relf = os.path.relpath(f, path)
                print "Adding file: %s"%relf
                ziph.write(f, relf)

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", help="The source folder to read from", action="store")
    parser.add_argument("--target", help="THe target folder to place the module in", action="store")
    args = parser.parse_args()
    mod = read_json(os.path.join(args.source, 'module.json'))
    print "Creating %s.zip"%mod["name"]
    zipf = zipfile.ZipFile(os.path.join(args.target, '%s.zip'%mod["name"]), 'w', zipfile.ZIP_DEFLATED)
    zipdir(args.source, zipf)
    zipf.close()