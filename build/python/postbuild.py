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
	list = tfile.split('-')
	if len(list) < 3:
		print 'Ignoring invalid file: %s'%name
		return
	
	name = list[0]
	version = list[1]
	arch = list[2]
	target = None
	print 'Found installer %s'%name
	target = find_target(name)
	if target:
		print 'Found tagged installer %s for %s'%(tfile, target)
		print 'Uploading name: %s to %s'%(tfile, target)
		os.system("%s %s %s"%(SCP_BINARY, file, target))
	else:
		print 'Ignoring unconfigured tagged installer: %s (TARGET_SITE=%s=URL;...)'%(name, name)

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

try:
	if TARGET_SITE != '' and SCP_BINARY != '':
		print "Distributing files to site..."

		for f in find_by_pattern(BUILD_TARGET_EXE_PATH, '*%s*.msi'%vstring):
			scp_file(f)
		for f in find_by_pattern(BUILD_TARGET_EXE_PATH, '*%s*.zip'%vstring):
			scp_file(f)
except NameError, e:
	print 'TARGET_SITE not defined so we wont upload anything: %s'%e

	
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
	files = [ 'nscp-%s-%s.zip'%(vstring, VERSION_ARCH) ]
	if BREAKPAD_FOUND == "TRUE":
		files.append(target_name)

	for a in release.iter_assets():
		if a.name in files:
			print "Deleting old file %s..."%name
			a.delete()
	for f in files:
		print "Uploading %s..."%f
		with open(f, "rb") as fd:
			release.upload_asset('application/zip', f, fd.read())
		
create_release()
