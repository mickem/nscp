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
SET(LIBRARY_ROOT_FOLDER	"${root}")
set(Boost_USE_STATIC_LIBS		ON)
set(Boost_USE_STATIC_RUNTIME	ON)
set(BOOST_USE_MULTITHREADED		ON)
SET(BOOST_ROOT "$${LIBRARY_ROOT_FOLDER}/${boost}")
SET(BOOST_LIBRARYDIR "$${BOOST_ROOT}/stage/lib")
SET(TINYXML2_DIR "$${LIBRARY_ROOT_FOLDER}/${TinyXML2}")
#SET(PROTOC_GEN_LUA "C:/Python/27x64/Scripts/")
#SET(PROTOBUF_LIBRARY_SUFFIX "-lite")
SET(PROTOBUF_ROOT "$${LIBRARY_ROOT_FOLDER}/${protobuf}")
SET(GTEST_ROOT "$${LIBRARY_ROOT_FOLDER}/${gtest}")
SET(OPENSSL_ROOT_DIR "$${LIBRARY_ROOT_FOLDER}/${openssl}/out32")
SET(_OPENSSL_INCLUDEDIR "$${LIBRARY_ROOT_FOLDER}/${openssl}/include")
SET(ZEROMQ_ROOT "$${LIBRARY_ROOT_FOLDER}/${ZeroMQ}")
SET(CRYPTOPP_DIR "$${LIBRARY_ROOT_FOLDER}/${cryptopp}")
SET(LUA_ROOT "$${LIBRARY_ROOT_FOLDER}/${lua}")
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
		for f in file.namelist():
			(root, ignored) = f.split('/', 1)
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
	'cryptopp',
	'lua',
	'boost',
	'openssl',
	'protobuf',
	'TinyXML2',
	'ZeroMQ',
	'breakpad',
	'gtest'
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
	specific_w32 = []
	specific_x64 = []
	common_post = []
	def __init__(self, common_pre, specific_w32, specific_x64, common_post):
		self.common_pre = common_pre
		self.specific_w32 = specific_w32
		self.specific_x64 = specific_x64
		self.common_post = common_post

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
					print('INFO Running: %s'%cmd)
					cmd = cmd.replace('$$NSCP_SOURCE_ROOT$$', source)
					cmd = cmd.replace('$$MSVER$$', msver)
					ret = os.system(cmd)
					if ret != 0:
						print "ERR  Failed to execute: %s (%d)"%(cmd, ret)
						log.close()
						sys.exit(1)
					print('----------------------------------------------------------------')
					print('INFO DONE: %s'%cmd)
					log.write(cached_name)

	def exec_build(self, folder, source, pre, task, post):
		cwd = os.getcwd()
		os.chdir(folder)
		self.exec_chunk('pre', pre, source)
		self.exec_chunk('task', task, source)
		self.exec_chunk('post', post, source)
		os.chdir(cwd)

	def build_w32(self, folder, source):
		self.exec_build(folder, source, self.common_pre, self.specific_w32, self.common_post)

	def build_x64(self, folder, source):
		self.exec_build(folder, source, self.common_pre, self.specific_x64, self.common_post)

sources = {}
sources['cryptopp'] = source('cryptopp561.zip', 'http://www.cryptopp.com/cryptopp561.zip', '31dbb456c21f50865218c57b7eaf4c955a222ba1')
sources['cryptopp'].folder = 'cryptopp-5.6.1'
# sources['lua'] = source('lua-5.2.1.tar.gz', 'http://www.lua.org/ftp/lua-5.2.1.tar.gz')
sources['lua'] = source('lua-5.1.5.tar.gz', 'http://www.lua.org/ftp/lua-5.1.5.tar.gz')

# sources['boost'] = source('boost_1_49_0.zip', 'http://sourceforge.net/projects/boost/files/boost/1.49.0/boost_1_49_0.zip/download', 'e2e778a444e7ae7157b97f4f1fedf363cf87940c')
sources['boost'] = source('boost_1_47_0.zip', 'http://sourceforge.net/projects/boost/files/boost/1.47.0/boost_1_47_0.zip/download', '06ce149fe2c3052ffb8dc79bbd0e61a7da011162')
sources['openssl'] = source('openssl-1.0.1c.tar.gz', 'http://www.openssl.org/source/openssl-1.0.1c.tar.gz', '91b684de947cb021ac61b8c51027cc4b63d894ce')
sources['protobuf'] = source('protobuf-2.4.1.tar.gz', 'http://protobuf.googlecode.com/files/protobuf-2.4.1.tar.gz', 'efc84249525007b1e3105084ea27e3273f7cbfb0')
sources['TinyXML2'] = source('tinyxml2-master.zip', 'https://github.com/leethomason/tinyxml2/zipball/master')
sources['ZeroMQ'] = source('zeromq-2.2.0.zip', 'http://download.zeromq.org/zeromq-2.2.0.zip')
sources['gtest'] = source('gtest-1.6.0.zip', 'http://googletest.googlecode.com/files/gtest-1.6.0.zip', '00d6be170eb9fc3b2198ffdcb1f1d6ba7fc6e621')

build = {}
post_build = {}


build['boost'] = build_instruction(
	['bootstrap.bat'], 
	['bjam --toolset=msvc-8.0 runtime-link=static'],
	['bjam --toolset=msvc-8.0 runtime-link=static address-model=64'],
	[]
	)

build['openssl'] = build_instruction(
	[], 
	['perl Configure VC-WIN32', 'ms\\do_ms'],
	['perl Configure VC-WIN64A', 'ms\\do_win64a'],
	['nmake -f ms\\nt.mak']
	)

build['protobuf'] = build_instruction(
	['python.exe $$NSCP_SOURCE_ROOT$$/build/python/msdev-to-x.py $$MSVER$$', 'python.exe $$NSCP_SOURCE_ROOT$$/build/python/msdev-to-static.py'], 
	[],
	['python.exe $$NSCP_SOURCE_ROOT$$/build/python/msdev-to-x64.py'],
	['msbuild vsprojects\\protobuf.sln /p:Configuration=Release', 'msbuild vsprojects\\protobuf.sln /p:Configuration=Debug']
	)

build['ZeroMQ'] = build_instruction(
	['python.exe $$NSCP_SOURCE_ROOT$$/build/python/msdev-to-x.py $$MSVER$$', 'python.exe $$NSCP_SOURCE_ROOT$$/build/python/msdev-to-static.py'
#		,'$$TODO: Apply debug patch$$'
		], 
	[],
	['python.exe $$NSCP_SOURCE_ROOT$$/build/python/msdev-to-x64.py'],
	['msbuild builds\\msvc\\msvc.sln /p:Configuration=Release', 'msbuild builds\\msvc\\msvc.sln /p:Configuration=Release']
	)

build['cryptopp'] = build_instruction(
	[], 
	['msbuild cryptlib.vcproj /p:Configuration=Release', 'msbuild cryptlib.vcproj /p:Configuration=Debug'],
	['msbuild cryptlib.vcproj /p:Configuration=Release /p:Platform=x64', 'msbuild cryptlib.vcproj /p:Configuration=Debug /p:Platform=x64'],
	[]
	)
build['gtest'] = build_instruction(
	[],
	['cmd /c "cmake . -Dgtest_disable_pthreads=true -G "Visual Studio 8 2005" & cmake . -Dgtest_disable_pthreads=true -G "Visual Studio 8 2005" & exit /b0"'],
	['cmd /c "cmake . -Dgtest_disable_pthreads=true -G "Visual Studio 8 2005 Win64" & cmake . -Dgtest_disable_pthreads=true -G "Visual Studio 8 2005 Win64" & exit /b0"'],
	['msbuild gtest.sln /p:Configuration=Release', 'msbuild gtest.sln /p:Configuration=Debug']
	)


post_build['protobuf'] = """Be sure to install protocol buffers python library in your python installation (notice if you have multiple you need to do this for all of them):
cd ${protobuf_abs}\python
c:\path\of\python.exe setup.py install"""
post_build['breakpad'] = """Google breakpad requires the plattform SDK to be able to build so you need to buildthat manually."""
post_build['global'] = """Validate your setup using the following command (notice boost will probably fail since it does not know which compiler you are using):
cmake -D TARGET=${target} -D SOURCE=${source} -P ${source}\check_deps.cmake

Then you need to build:
cd ${target}
cmake -G "Visual Studio 8 2005" ../${source}
or
cmake -G "Visual Studio 8 2005 Win64" ../${source}

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

def build_source(root, target, source):
	for t in targets:
		if t in build:
			folder = root
			if t in sources:
				folder = sources[t].get_target_folder(root)
			b = build[t]
			if target == 'x64':
				b.build_x64(folder, source)
			else:
				b.build_w32(folder, source)
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

def post_build_source(root, target, source):
	data = {}
	data['root'] = root.replace('\\', '/')
	data['target'] = target.replace('\\', '/')
	data['source'] = source.replace('\\', '/')
	for t in targets:
		if t in sources:
			data[t] = sources[t].find_rel_path(root)
			data['%s_abs'%t] = sources[t].find_abs_path(root)
	local_targets = targets
	local_targets.append('global')
	for t in local_targets:
		if t in post_build:
			tpl = Template(post_build[t])
			print "---/// %s ///---"%t
			print tpl.safe_substitute(data)

parser = OptionParser()
parser.add_option("-d", "--directory", help="Folder to build in (defaults to current)")
parser.add_option("-t", "--target", help="Which target architecture to build (win32 or x64)")
parser.add_option("-c", "--cmake-config", help="Folder to place cmake configuration file in")
parser.add_option("-D", "--cmake-define", action="append", help="Set other variables in the cmake config file")
parser.add_option("-s", "--source", help="Location of the nscp source folder")
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
build_source(options.directory, options.target, options.source)
if options.cmake_config:
	write_config(options.directory, options.cmake_config, options.cmake_define)
	post_build_source(options.directory, options.cmake_config, options.source)
else:
	print 'WARN Since you did not specify --cmake-config we will not write the cmake config file so most likely this was not very usefull...'
