import urllib
import os
import sys
import hashlib
import zipfile
import tarfile
from optparse import OptionParser
from string import Template

msver = '2005'
CONFIG_TEMPLATE = """
SET(USE_STATIC_RUNTIME FALSE)
SET(LIBRARY_ROOT_FOLDER	"${root}")
SET(BOOST_ROOT "$${LIBRARY_ROOT_FOLDER}/${boost}")
SET(BOOST_LIBRARYDIR "$${BOOST_ROOT}/stage/lib")
SET(PROTOBUF_ROOT "$${LIBRARY_ROOT_FOLDER}/${protobuf}")
SET(OPENSSL_ROOT_DIR "$${LIBRARY_ROOT_FOLDER}/${openssl}/out32")
SET(_OPENSSL_INCLUDEDIR "$${LIBRARY_ROOT_FOLDER}/${openssl}/include")
SET(LUA_ROOT "$${LIBRARY_ROOT_FOLDER}/${lua}")
#SET(PYTHON_ROOT "TODO")
#SET(BREAKPAD_ROOT "TODO")
#SET(ARCHIVE_FOLDER "TODO")
#SET(TARGET_SITE "TODO")
"""

def find_compressor(path):
	if path.endswith('.zip'):
		opener, mode = zipfile.ZipFile, 'r'
	elif path.endswith('.tar.gz') or path.endswith('.tgz'):
		opener, mode = tarfile.open, 'r:gz'
	elif path.endswith('.tar.bz2') or path.endswith('.tbz'):
		opener, mode = tarfile.open, 'r:bz2'
	else: 
		raise ValueError, "Could not extract `%s` as no appropriate extractor is found" % path
	return (opener, mode)

def extract_file(path, to_directory='.'):
	(opener, mode) = find_compressor(path)
	cwd = os.getcwd()
	os.chdir(to_directory)

	try:
		file = opener(path, mode)
		try: file.extractall()
		finally: file.close()
	finally:
		os.chdir(cwd)

def get_root_from_file(path):
	(opener, mode) = find_compressor(path)
	file = opener(path, mode)
	try:
		roots = []
		list = []
		if hasattr(file, 'namelist'):
			list = file.namelist()
		elif hasattr(file, 'getnames'):
			list = file.getnames()
		else:
			print 'ERR  Failed to retrive data from: %s'%path
			return None
		for f in list:
			if '/' in f:
				(root, ignored) = f.split('/', 1)
			else:
				root = f
			if root not in roots:
				roots.append(root)
		if len(roots) == 1:
			return roots[0]
		else:
			return None
	finally:
		file.close()
	return None

targets = [
	'lua',
	'boost',
	'openssl',
	'protobuf',
	'breakpad'
]

class source:
	file = None
	url = None
	hash = None
	folder = '.'
	target_folder = None
	def __init__(self, file, url, hash = None):
		self.file = file
		self.url = url
		self.hash = hash
		self.folder = '.'
		self.target_folder = None

	def calculate_hash(self, folder):
		if not self.hash:
			return None
		file = os.path.join(folder,self.file)
		f = open(file, 'rb')
		try:
			hasher = hashlib.sha1()
			hasher.update(f.read())
			return hasher.hexdigest()
		finally:
			f.close()
		return None

	def is_cached(self, folder):
		file = os.path.join(folder,self.file)
		return os.path.exists(file)

	def has_hash(self):
		return self.hash

	def decompress(self, folder):
		expected_folder = self.get_target_folder(folder)
		if os.path.exists(expected_folder):
			print 'OK   Found cached folder (assuming it is good): %s'%expected_folder
			return
		file = os.path.join(folder,self.file)
		folder = os.path.join(folder,self.folder)
		if not os.path.exists(folder):
			os.mkdir(folder)
		try:
			extract_file(file, folder)
		except:
			print 'ERR  Failed to decompress: %s into %s'%(file, folder)
		
	def fetch(self, folder):
		file = os.path.join(folder,self.file)
		urllib.urlretrieve(self.url, file)

	def get_target_folder(self, folder):
		if self.target_folder:
			return self.target_folder
		self.target_folder = self.folder
		if not self.target_folder or self.target_folder == '.':
			zip_file = os.path.join(folder,self.file)
			try:
				folder = get_root_from_file(zip_file)
				if folder:
					self.target_folder = folder
					return self.target_folder
			except Exception as err:
				print 'ERR  Failed to inspect %s: %s'%(zip_file, err)
		
			(self.target_folder, ext) = os.path.splitext(self.file)
			if self.target_folder.endswith('.tar'):
				(self.target_folder, ext) = os.path.splitext(self.target_folder)
		return self.target_folder

	def find_rel_path(self, folder):
		return self.get_target_folder(folder)

	def find_abs_path(self, folder):
		return os.path.join(folder,self.get_target_folder(folder))

class build_instruction:
	common_pre = []
	pre_w32 = []
	pre_x64 = []
	specific_w32 = []
	specific_x64 = []
	common_post = []
	def __init__(self, common_pre, specific_w32, specific_x64, common_post):
		self.common_pre = common_pre
		self.specific_w32 = specific_w32
		self.specific_x64 = specific_x64
		self.common_post = common_post
		self.pre_w32 = []
		self.pre_x64 = []

	def exec_chunk(self, tag, chunk, source):
		global msver
		lines = []
		log = None
		if os.path.exists('nscp.command.log'):
			with open('nscp.command.log', "r") as sources:
				lines = sources.readlines()
		with open('nscp.command.log', "a") as log:
			for cmd in chunk:
				cached_name = '%s: %s\n'%(tag, cmd)
				if cached_name in lines:
					print('INFO found cached: %s (not running)'%cmd)
				else:
					raw_cmd = cmd
					cmd = cmd.replace('$$NSCP_SOURCE_ROOT$$', source)
					cmd = cmd.replace('$$MSVER$$', msver)
					cmd = cmd.replace('$boost-version$', boost_version[msver])
					cmd = cmd.replace('$$CMAKE_GENERATOR$$', cmake_generator[msver])
					print('INFO Running: "%s" as "%s"'%(raw_cmd, cmd))
					
					if msver == '2012':
						cmd = cmd.replace('.vcproj', '.vcxproj')
					ret = os.system(cmd)
					if ret != 0:
						print "ERR  Failed to execute: %s (%d)"%(cmd, ret)
						log.close()
						sys.exit(1)
					print('----------------------------------------------------------------')
					print('INFO DONE: %s'%cmd)
					log.write(cached_name)

	def exec_build(self, folder, source, pre1, pre2, task, post):
		cwd = os.getcwd()
		os.chdir(folder)
		self.exec_chunk('pre', pre1, source)
		self.exec_chunk('pre', pre2, source)
		self.exec_chunk('task', task, source)
		self.exec_chunk('post', post, source)
		os.chdir(cwd)

	def build_w32(self, folder, source):
		self.exec_build(folder, source, self.pre_w32, self.common_pre, self.specific_w32, self.common_post)

	def build_x64(self, folder, source):
		self.exec_build(folder, source, self.pre_x64, self.common_pre, self.specific_x64, self.common_post)

sources = {}
# sources['lua'] = source('lua-5.2.1.tar.gz', 'http://www.lua.org/ftp/lua-5.2.1.tar.gz')
sources['lua'] = source('lua-5.1.5.tar.gz', 'http://www.lua.org/ftp/lua-5.1.5.tar.gz')

sources['boost'] = source('boost_1_52_0.zip', 'http://sourceforge.net/projects/boost/files/boost/1.52.0/boost_1_52_0.zip/download', '5cfe29351e0b734993a2e6717e22b709680dd132')

sources['openssl'] = source('openssl-1.0.1h.tar.gz', 'https://www.openssl.org/source/openssl-1.0.1h.tar.gz', 'b2239599c8bf8f7fc48590a55205c26abe560bf8')

sources['protobuf'] = source('protobuf-2.4.1.tar.gz', 'http://protobuf.googlecode.com/files/protobuf-2.4.1.tar.gz', 'efc84249525007b1e3105084ea27e3273f7cbfb0')

build = {}
post_build = {}


build['boost'] = build_instruction(
	['bootstrap.bat'], 
	['bjam --toolset=$boost-version$ runtime-link=shared link=shared', 'bjam --toolset=$boost-version$ runtime-link=static'],
	['bjam --toolset=$boost-version$ runtime-link=shared link=shared address-model=64', 'bjam --toolset=$boost-version$ runtime-link=static address-model=64'],
	[]
	)

build['openssl-d'] = build_instruction(
	[], 
	['perl Configure VC-WIN32', 'ms\\do_ms'],
	['perl Configure VC-WIN64A', 'ms\\do_win64a'],
	['nmake -f ms\\ntdll.mak']
	)
build['openssl-s'] = build_instruction(
	[], 
	['perl Configure VC-WIN32', 'ms\\do_ms'],
	['perl Configure VC-WIN64A', 'ms\\do_win64a'],
	['nmake -f ms\\nt.mak']
	)

build['protobuf-d'] = build_instruction(
	['python.exe $$NSCP_SOURCE_ROOT$$/build/python/msdev-to-x.py $$MSVER$$'], 
	[],
	[],
	[
		'msbuild vsprojects\\libprotobuf.vcxproj /p:Configuration=Release',
		'msbuild vsprojects\\libprotobuf.vcxproj /p:Configuration=Debug',
		'msbuild vsprojects\\libprotoc.vcxproj /p:Configuration=Release',
		'msbuild vsprojects\\libprotoc.vcxproj /p:Configuration=Debug',
		'msbuild vsprojects\\protoc.vcxproj /p:Configuration=Release',
		'msbuild vsprojects\\protoc.vcxproj /p:Configuration=Debug',
		'msbuild vsprojects\\libprotobuf-lite.vcxproj /p:Configuration=Release',
		'msbuild vsprojects\\libprotobuf-lite.vcxproj /p:Configuration=Debug'
	]
	)
build['protobuf-d'].pre_x64.append('python.exe $$NSCP_SOURCE_ROOT$$/build/python/msdev-to-x64.py')
build['protobuf-s'] = build_instruction(
	['python.exe $$NSCP_SOURCE_ROOT$$/build/python/msdev-to-static.py', 'python.exe $$NSCP_SOURCE_ROOT$$/build/python/msdev-to-x.py $$MSVER$$'], 
	[],
	[],
	[
		'msbuild vsprojects\\libprotobuf.vcxproj /p:Configuration=Release',
		'msbuild vsprojects\\libprotobuf.vcxproj /p:Configuration=Debug',
		'msbuild vsprojects\\libprotoc.vcxproj /p:Configuration=Release',
		'msbuild vsprojects\\libprotoc.vcxproj /p:Configuration=Debug',
		'msbuild vsprojects\\protoc.vcxproj /p:Configuration=Release',
		'msbuild vsprojects\\protoc.vcxproj /p:Configuration=Debug',
		'msbuild vsprojects\\libprotobuf-lite.vcxproj /p:Configuration=Release',
		'msbuild vsprojects\\libprotobuf-lite.vcxproj /p:Configuration=Debug'
	]
	)
build['protobuf-s'].pre_x64.append('python.exe $$NSCP_SOURCE_ROOT$$/build/python/msdev-to-x64.py')

boost_version = {}
boost_version['2005'] = "msvc-8.0"
boost_version['2012'] = "msvc-11.0"
cmake_generator = {}
cmake_generator['2005'] = "Visual Studio 8 2005"
cmake_generator['2012'] = "Visual Studio 11"

post_build['protobuf'] = """Be sure to install protocol buffers python library in your python installation (notice if you have multiple you need to do this for all of them):
cd ${protobuf_abs}\python
c:\path\of\python.exe setup.py install"""
post_build['breakpad'] = """Google breakpad requires the plattform SDK to be able to build so you need to buildthat manually."""
post_build['validate'] = """You can (if you wich) validate your setup using the following command:
cmake -D TARGET=${target} -D SOURCE=${source} -P ${source}\check_deps.cmake
Notice: boost will probably fail since it does not know which compiler you are using"""
post_build['nsclient++'] = """To build NSClient++ you can run the following commands
cd ${target}
cmake -G "${cmake_generator}" ${source}
"""


def fetch_sources(root):
	for t in targets:
		if t in sources:
			source = sources[t]
			if not source.is_cached(root):
				print 'INFO Retrieving: %s from %s'%(t, source.url)
				source.fetch(root)
		else:
			print 'ERR  %s has to be manually downloaded'%t

def decompress_sources(root):
	for t in targets:
		if t in sources:
			source = sources[t]
			if not source.is_cached(root):
				print 'ERR  Missing dependency found: %s was missing %s'%(t, file)
				sys.exit(1)
			if source.has_hash():
				calculated_hash = source.calculate_hash(root)
				if calculated_hash == source.hash:
					print 'OK   Validated sha for %s'%t
				else:
					print 'ERR  Invalid hash for: %s (%s) %s != %s'%(t, source.file, calculated_hash, source.hash)
					sys.exit(1)
			else:
				print 'WARN no sha defined for: %s (%s)'%(t, source.file)
			print 'OK   Uncompressing: %s into %s'%(t, source.folder)
			source.decompress(root)
		else:
			print 'ERR  %s has to be manually downloaded'%t

def build_source(root, target, source, dyn):
	for t in targets:
		suffix = 's'
		if dyn:
			suffix = 'd'
		key = '%s-%s'%(t,suffix)
		builder = None
		if key in build:
			builder = build[key]
		elif t in build:
			builder = build[t]

		if builder:
			folder = root
			if t in sources:
				folder = sources[t].get_target_folder(root)
			if target == 'x64':
				builder.build_x64(folder, source)
			else:
				builder.build_w32(folder, source)
		else:
			print 'OK   %s does not require building'%t

def write_config(root, target, defines):
	data = {}
	tpl = Template(CONFIG_TEMPLATE)
	data['root'] = root.replace('\\', '/')
	data['target'] = target.replace('\\', '/')
	for t in targets:
		if t in sources:
			data[t] = sources[t].find_rel_path(root)
			data['%s_abs'%t] = sources[t].find_abs_path(root)
	config = tpl.safe_substitute(data)
	if not os.path.exists(target):
		os.mkdir(target)

	with open(os.path.join(target, 'build.cmake'), "w") as conf:
		conf.write(config)
		if defines:
			for define in defines:
				(key, value) = define.split('=')
				conf.write('SET(%s "%s")'%(key, value))
	print 'OK   CMake config written to: %s'%target

def post_build_source(root, target, arch, source):
	global msver
	data = {}
	data['root'] = root.replace('\\', '/')
	data['target'] = target.replace('\\', '/')
	data['source'] = source.replace('\\', '/')
	data['cmake_generator'] = cmake_generator[msver]
	if arch == 'x64':
		data['cmake_generator'] = '%s Win64'%data['cmake_generator']
	for t in targets:
		if t in sources:
			data[t] = sources[t].find_rel_path(root)
			data['%s_abs'%t] = sources[t].find_abs_path(root)
	local_targets = targets
	local_targets.append('validate')
	local_targets.append('nsclient++')
	for t in local_targets:
		if t in post_build:
			tpl = Template(post_build[t])
			print
			print " * %s"%t.upper()
			print '   ' + tpl.safe_substitute(data).replace('\n', '\n   ')

parser = OptionParser()
parser.add_option("-d", "--directory", help="Folder to build in (defaults to current)")
parser.add_option("-t", "--target", help="Which target architecture to build (win32 or x64)")
parser.add_option("-c", "--cmake-config", help="Folder to place cmake configuration file in")
parser.add_option("-D", "--cmake-define", action="append", help="Set other variables in the cmake config file")
parser.add_option("-s", "--source", help="Location of the nscp source folder")
parser.add_option("-y", "--dyn", action="store_true", help="Use dynamic link")
parser.add_option("--msver", default='2005', help="Which version of visual studio you have")

(options, args) = parser.parse_args()

if not options.directory:
	options.directory = os.getcwd()
else:
	options.directory = os.path.abspath(options.directory)
if not options.source:
	options.source = os.path.dirname(os.path.dirname(os.path.dirname(sys.argv[0])))

msver = options.msver

if options.source:
	options.source = os.path.abspath(options.source)

if not os.path.exists(options.source):
	print 'ERR  NSCP source folder was not found (strange, thats where this script should be right?) --source=%s'%options.source
	sys.exit(1)

fetch_sources(options.directory)
decompress_sources(options.directory)
build_source(options.directory, options.target, options.source, options.dyn)
if options.cmake_config:
	write_config(options.directory, options.cmake_config, options.cmake_define)
	post_build_source(options.directory, options.cmake_config, options.target, options.source)
else:
	print 'WARN Since you did not specify --cmake-config we will not write the cmake config file so most likely this was not very usefull...'
