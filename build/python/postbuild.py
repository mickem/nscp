import sys
import os
import zipfile
import fnmatch
import shutil


BREAKPAD_FOUND = "${BREAKPAD_FOUND}"
BREAKPAD_FOUND = "FALSE"
BREAKPAD_DUMPSYMS_EXE = "${BREAKPAD_DUMPSYMS_EXE}"
BUILD_TARGET_EXE_PATH = "${BUILD_TARGET_EXE_PATH}"
BUILD_PYTHON_FOLDER = "${BUILD_PYTHON_FOLDER}"
VERSION = "${VERSION_SERIES}.${VERSION_MAJOR}.${VERSION_MINOR}"
CMAKE_CL_64 = "${CMAKE_CL_64}"
ARCHIVE_FOLDER = "${ARCHIVE_FOLDER}"
VERSION_TXT = "${VERSION_TXT}"
VERSION_ARCH = "${VERSION_ARCH}"

sys.path.append(BUILD_PYTHON_FOLDER)
from VersionHandler import VersionHandler

version = VersionHandler(VERSION_TXT)
version.read()
datestr = version.datestr()

def rename_and_move(file, target):
	tfile = '%s/%s'%(target, os.path.basename(file))
	tfile = tfile.replace('win64', 'x64')
	print "Copying %s to %s"%(file, tfile)
	shutil.copy(file, tfile)

def find_by_pattern(path, pattern):
	matches = []
	for root, dirnames, filenames in os.walk(path):
		if '_CPack_Packages' not in root:
			for filename in fnmatch.filter(filenames, pattern):
				matches.append(os.path.join(root, filename))
	return matches
	
target_name = 'NSCP-%s-%s-symbols.zip'%(VERSION, VERSION_ARCH)

if BREAKPAD_FOUND == "TRUEee":
	print "Gathering symbols into %s"%target_name
	matches = find_by_pattern(BUILD_TARGET_EXE_PATH, '*.pdb')
	zip = zipfile.ZipFile(target_name, "w")
	for f in matches:
		print "Processing: %s"%f
		out = f.replace('.pdb', '.sym')
		os.system("%s %s > %s"%(BREAKPAD_DUMPSYMS_EXE, f, out))
		if out.startswith(BUILD_TARGET_EXE_PATH):
			name = out[len(BUILD_TARGET_EXE_PATH)+1:]
		else:
			name = out
		zip.write(out, name)
	zip.close()

if ARCHIVE_FOLDER != "":
	print "Archiving files..."
	target_installer = "%s/%s"%(ARCHIVE_FOLDER,"installers")
	target_archives = "%s/%s"%(ARCHIVE_FOLDER,"archive")
	if not os.path.isdir(ARCHIVE_FOLDER):
		os.mkdir(ARCHIVE_FOLDER)
	if not os.path.isdir(target_installer):
		os.mkdir(target_installer)
	if not os.path.isdir(target_archives):
		os.mkdir(target_archives)

	for f in find_by_pattern(BUILD_TARGET_EXE_PATH, '*.msi'):
		rename_and_move(f, target_installer)
	for f in find_by_pattern(BUILD_TARGET_EXE_PATH, '*.zip'):
		rename_and_move(f, target_archives)
	

