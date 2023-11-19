import sys
import os
import zipfile
import fnmatch
import shutil

from config import BUILD_PYTHON_FOLDER, VERSION_TXT, VERSION_ARCH, BUILD_TARGET_EXE_PATH, \
  DOCS_FOLDER, BREAKPAD_FOUND, BREAKPAD_DUMPSYMS_EXE, ARCHIVE_FOLDER

sys.path.append(BUILD_PYTHON_FOLDER)
from VersionHandler import VersionHandler

version = VersionHandler(VERSION_TXT)
version.read()
datestr = version.datestr()
vstring = version.version()

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
	
target_docs = 'NSCP-%s-%s-docs.zip'%(vstring, VERSION_ARCH)
print("Gathering docs into %s"%target_docs)
docs_folder_root = os.path.join(BUILD_TARGET_EXE_PATH, DOCS_FOLDER)
matches = find_by_pattern(docs_folder_root, '*.*')
zip = zipfile.ZipFile(target_docs, 'w', zipfile.ZIP_DEFLATED)
for f in matches:
	name = os.path.relpath(f, docs_folder_root)
	zip.write(f, name)
zip.close()

target_name = 'NSCP-%s-%s-symbols.zip'%(vstring, VERSION_ARCH)
if BREAKPAD_FOUND == "TRUE":
	print("Gathering symbols into %s"%target_name)
	matches = find_by_pattern(BUILD_TARGET_EXE_PATH, '*.pdb')
	zip = zipfile.ZipFile(target_name, 'w', zipfile.ZIP_DEFLATED)
	for f in matches:
		print("Processing: %s"%f)
		out = f.replace('.pdb', '.sym')
		os.system("%s %s > %s"%(BREAKPAD_DUMPSYMS_EXE, f, out))
		name = 'invalid/%s'%os.path.basename(out)
		with open(out, 'r') as f:
			head = f.readline().strip()
			try:
				# MODULE windows x86 1FD4DBADB2B446CA81E0F689BE0FFCA61c nscp.pdb
				(module, tos, tarch, guid, name) = head.split(' ')
				name = 'NSCP.breakpad.syms/%s/%s/%s'%(name, guid, os.path.basename(out))
			except:
				print('Error failed to parse: %s'%out)
			
		zip.write(out, name)
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

	for f in find_by_pattern(BUILD_TARGET_EXE_PATH, '*%s*.msi'%vstring):
		rename_and_move(f, target_installer)
	for f in find_by_pattern(BUILD_TARGET_EXE_PATH, '*%s*.zip'%vstring):
		rename_and_move(f, target_archives)

