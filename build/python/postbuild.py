import sys
import os
import zipfile
import fnmatch
import shutil


execfile("config.py")

sys.path.append(BUILD_PYTHON_FOLDER)
from VersionHandler import VersionHandler

version = VersionHandler(VERSION_TXT)
version.read()
datestr = version.datestr()
vstring = version.version()

targets = None

def build_targets(str):
	targets = {}
	for itm in str.split(';'):
		tmp = itm.split('=')
		if len(tmp) == 0:
			continue
		elif len(tmp) == 1:
			targets['nscp'] = tmp[0]
		else:
			targets[tmp[0]] = tmp[1]
	return targets
			
	
		
def find_target(key=None):
	global targets
	if not targets:
		targets = build_targets(TARGET_SITE)
	if not key:
		key = 'nscp'
	if not key in targets:
		return None
	return targets[key]


def scp_file(file):
	tfile = os.path.basename(file)
	(name, version, arch) = tfile.split('-')
	target = None
	if '_' in name:
		(name, tag) = name.split('_')
		target = find_target(tag)
		if target:
			print 'Found tagged installer %s for %s'%(tfile, tag)
		else:
			print 'Ignoring unconfigured tagged installer: %s (define TARGET_SITE_%s to a destination)'%(tag,tag)
	else:
		print 'Found normal installer %s'%tfile
		target = find_target()
	if target:
		print 'Uploading name: %s to %s'%(tfile, target)
		os.system("%s %s %s"%(SCP_BINARY, file, target))

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
	
target_name = 'NSCP-%s-%s_symbols.zip'%(vstring, VERSION_ARCH)

if BREAKPAD_FOUND == "TRUE":
	print "Gathering symbols into %s"%target_name
	matches = find_by_pattern(BUILD_TARGET_EXE_PATH, '*.pdb')
	zip = zipfile.ZipFile(target_name, 'w', zipfile.ZIP_DEFLATED)
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

	for f in find_by_pattern(BUILD_TARGET_EXE_PATH, '*%s*.msi'%vstring):
		rename_and_move(f, target_installer)
	for f in find_by_pattern(BUILD_TARGET_EXE_PATH, '*%s*.zip'%vstring):
		rename_and_move(f, target_archives)

try:
	if TARGET_SITE != '' and SCP_BINARY != '':
		print "Distributing files to site..."

		for f in find_by_pattern(BUILD_TARGET_EXE_PATH, '*%s*.msi'%vstring):
			scp_file(f)
		for f in find_by_pattern(BUILD_TARGET_EXE_PATH, '*%s*.zip'%vstring):
			scp_file(f)
except NameError:
	print 'TARGET_SITE not defined so we wont upload anything...'
