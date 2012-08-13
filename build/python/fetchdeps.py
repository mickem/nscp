import urllib
import os
import sys
import hashlib
import zipfile
import tarfile
from optparse import OptionParser
from string import Template

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
SET(OPENSSL_ROOT_DIR "$${LIBRARY_ROOT_FOLDER}/${openssl}/out32dll")
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
	'google breakpad'
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

	def exec_chunk(self, tag, chunk):
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
					ret = os.system(cmd.replace('$$NSCP_SOURCE_ROOT$$', 'D:\\source\\nscp\\trunk\\'))
					if ret != 0:
						print "ERR  Failed to execute: %s (%d)"%(cmd, ret)
						log.close()
						sys.exit(1)
					print('----------------------------------------------------------------')
					print('INFO DONE: %s'%cmd)
					log.write(cached_name)

	def exec_build(self, folder, pre, task, post):
		cwd = os.getcwd()
		os.chdir(folder)
		self.exec_chunk('pre', pre)
		self.exec_chunk('task', task)
		self.exec_chunk('post', post)
		os.chdir(cwd)

	def build_w32(self, folder):
		self.exec_build(folder, self.common_pre, self.specific_w32, self.common_post)

	def build_x64(self, folder):
		self.exec_build(folder, self.common_pre, self.specific_x64, self.common_post)

sources = {}
sources['cryptopp'] = source('cryptopp561.zip', 'http://www.cryptopp.com/cryptopp561.zip', '31dbb456c21f50865218c57b7eaf4c955a222ba1')
sources['cryptopp'].folder = 'cryptopp-5.6.1'
sources['lua'] = source('lua-5.2.1.tar.gz', 'http://www.lua.org/ftp/lua-5.2.1.tar.gz')
# sources['boost'] = source('boost_1_49_0.zip', 'http://sourceforge.net/projects/boost/files/boost/1.49.0/boost_1_49_0.zip/download', 'e2e778a444e7ae7157b97f4f1fedf363cf87940c')
sources['boost'] = source('boost_1_47_0.zip', 'http://sourceforge.net/projects/boost/files/boost/1.47.0/boost_1_47_0.zip/download', '06ce149fe2c3052ffb8dc79bbd0e61a7da011162')
sources['openssl'] = source('openssl-1.0.1c.tar.gz', 'http://www.openssl.org/source/openssl-1.0.1c.tar.gz', '91b684de947cb021ac61b8c51027cc4b63d894ce')
sources['protobuf'] = source('protobuf-2.4.1.tar.gz', 'http://protobuf.googlecode.com/files/protobuf-2.4.1.tar.gz', 'efc84249525007b1e3105084ea27e3273f7cbfb0')
sources['TinyXML2'] = source('tinyxml2-master.zip', 'https://github.com/leethomason/tinyxml2/zipball/master')
sources['ZeroMQ'] = source('zeromq-2.2.0.zip', 'http://download.zeromq.org/zeromq-2.2.0.zip')

build = {}
no_build = {}

no_build['lua'] = True
no_build['TinyXML2'] = True
no_build['cryptopp'] = True

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
	['nmake -f ms\\ntdll.mak']
	)

build['protobuf'] = build_instruction(
	['cd', 'python.exe $$NSCP_SOURCE_ROOT$$/build/python/msdev-to-2005.py', 'python.exe $$NSCP_SOURCE_ROOT$$/build/python/msdev-to-static.py'], 
	[],
	['$$TODO: Apply x64 patch$$'],
	['cd', 'msbuild vsprojects\\protobuf.sln /p:Configuration=Release', 'msbuild vsprojects\\protobuf.sln /p:Configuration=Debug']
	)

build['ZeroMQ'] = build_instruction(
	['python.exe $$NSCP_SOURCE_ROOT$$/build/python/msdev-to-2005.py', 'python.exe $$NSCP_SOURCE_ROOT$$/build/python/msdev-to-static.py'
#		,'$$TODO: Apply debug patch$$'
		], 
	[],
	['$$TODO: Apply x64 patch$$'],
	['msbuild builds\\msvc\\msvc.sln /p:Configuration=Release', 'msbuild builds\\msvc\\msvc.sln /p:Configuration=Release']
	)

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

def build_source(root, target):
	for t in targets:
		if t in no_build:
			print 'OK   %s does not require building'%t
		elif t in build:
			folder = root
			if t in sources:
				folder = sources[t].get_target_folder(root)
			b = build[t]
			if target == 'x64':
				b.build_x64(folder)
			else:
				b.build_w32(folder)
		else:
			print 'ERR  %s has to be built manually'%t


def write_config(root, target, defines):
	tpl = Template(CONFIG_TEMPLATE)
	data = {'root': root.replace('\\', '/')}
	for t in targets:
		if t in sources:
			data[t] = sources[t].find_rel_path(root)
			data['%s_abs'%t] = sources[t].find_abs_path(root)
	print data
	config = tpl.safe_substitute(data)
	if not os.path.exists(target):
		os.mkdir(target)

	with open(os.path.join(target, 'build.cmake'), "w") as conf:
		conf.write(config)
		for define in defines:
			(key, value) = define.split('=')
			conf.write('SET(%s "%s")'%(key, value))
	print 'OK   CMake config written to: %s'%target

parser = OptionParser()
parser.add_option("-d", "--directory", help="Folder to build in (defaults to current)")
parser.add_option("-t", "--target", help="Which target architecture to build (win32 or x64)")
parser.add_option("-c", "--cmake-config", help="Folder to place cmake configuration file in")
parser.add_option("-D", "--cmake-define", action="append", help="Set other variables in the cmake config file")

(options, args) = parser.parse_args()

if not options.directory:
	options.directory = os.getcwd()

fetch_sources(options.directory)
decompress_sources(options.directory)
build_source(options.directory, options.target)
if options.cmake_config:
	write_config(options.directory, options.cmake_config, options.cmake_define)
