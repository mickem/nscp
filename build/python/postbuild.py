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
				if filename == "vc110.pdb":
					continue
				matches.append(os.path.join(root, filename))
	return matches
	
target_docs = 'NSCP-%s-%s-docs.zip'%(vstring, VERSION_ARCH)
print "Gathering docs into %s"%target_docs
docs_folder_root = os.path.join(BUILD_TARGET_EXE_PATH, DOCS_FOLDER)
matches = find_by_pattern(docs_folder_root, '*.*')
zip = zipfile.ZipFile(target_docs, 'w', zipfile.ZIP_DEFLATED)
for f in matches:
	name = os.path.relpath(f, docs_folder_root)
	zip.write(f, name)
zip.close()

target_name = 'NSCP-%s-%s-symbols.zip'%(vstring, VERSION_ARCH)
if BREAKPAD_FOUND == "TRUE":
	print "Gathering symbols into %s"%target_name
	matches = find_by_pattern(BUILD_TARGET_EXE_PATH, '*.pdb')
	zip = zipfile.ZipFile(target_name, 'w', zipfile.ZIP_DEFLATED)
	for f in matches:
		print "Processing: %s"%f
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
				print 'Error failed to parse: %s'%out
			
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

def create_token():
	from github3 import authorize
	from getpass import getuser, getpass

	user = getuser()
	print "Enter github username [%s]:"%user,
	tmp = raw_input()
	if tmp:
		user = tmp
	password = ''

	while not password:
		password = getpass('Password for {0}: '.format(user))

	scopes = ['user', 'repo']
	auth = authorize(user, password, scopes, 'NSClient++ builder', 'http://nsclient.org')
	print auth.as_json()

	with open(CREDENTIALS_FILE, 'w') as fd:
		fd.write('%s\n'%auth.token)
		fd.write('%s'%auth.id)

def get_github_token():
	print "Connecting to github..."
	from github3 import login
	
	if not os.path.exists(CREDENTIALS_FILE):
		create_token()
	token = id = ''
	with open(CREDENTIALS_FILE, 'r') as fd:
		token = fd.readline().strip()  # Can't hurt to be paranoid
		id = fd.readline().strip()

	print "Logging in..."
	gh = login(token=token)
	return gh

def create_release():
	gh = get_github_token()
	repository = gh.repository('mickem', 'nscp')
	name = vstring
	release = None
	for r in repository.iter_releases():
		if r.name == name:
			release = r
			print "Found old release v%s..."%vstring
			break
	
	if not release:
		print "Creating release v%s..."%vstring
		release = repository.create_release('%s'%vstring, 'master', name, 'PLEASE UPDATE!', True)
	files = [ 'nscp-%s-%s.zip'%(vstring, VERSION_ARCH), target_docs ]
	if BREAKPAD_FOUND == "TRUE":
		files.append(target_name)

	for a in release.iter_assets():
		if a.name in files:
			print "Deleting old file %s..."%name
			a.delete()
	for f in files:
		print " + Uploading %s..."%f
		with open(f, "rb") as fd:
			release.upload_asset('application/zip', f, fd.read())
	for f in find_by_pattern(BUILD_TARGET_EXE_PATH, '*%s*.msi'%vstring):
		(path, fname) = os.path.split(f)
		for a in release.iter_assets():
			if a.name == fname:
				print "Deleting old file %s..."%fname
				a.delete()
		print " + Uploading %s..."%fname
		with open(f, "rb") as fd:
			release.upload_asset('application/zip', fname, fd.read())
for i in [1,2,3]:
	try:
		print "Pushing to github attempt %d/3"%i
		create_release()
		break
	except Exception, e:
		print "Failed to puload: %s" %e
		pass