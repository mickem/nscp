import urllib
import os
import sys
import hashlib
import zipfile
import tarfile
from optparse import OptionParser
from string import Template
import json

CONFIG_TEMPLATE = """
SET(USE_STATIC_RUNTIME FALSE)
SET(LIBRARY_ROOT_FOLDER	"${build_folder}")
SET(BOOST_ROOT "$${LIBRARY_ROOT_FOLDER}/${boost_rel_folder}")
SET(BOOST_LIBRARYDIR "$${BOOST_ROOT}/stage/lib")
SET(PROTOBUF_ROOT "$${LIBRARY_ROOT_FOLDER}/${protobuf_rel_folder}")
SET(OPENSSL_ROOT_DIR "$${LIBRARY_ROOT_FOLDER}/${openssl_rel_folder}/out32dll")
SET(_OPENSSL_INCLUDEDIR "$${LIBRARY_ROOT_FOLDER}/${openssl_rel_folder}/include")
SET(LUA_ROOT "$${LIBRARY_ROOT_FOLDER}/${lua_rel_folder}")
SET(BREAKPAD_ROOT "$${LIBRARY_ROOT_FOLDER}/${breakpad_rel_folder}")
SET(PYTHON_ROOT "${python_folder}")
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

class source_helper:
    file = None
    url = None
    hash = None
    def __init__(self, file, url, hash = None):
        self.file = file
        self.url = url
        self.hash = hash

    def calculate_hash(self):
        if not self.hash:
            return None
        with open(self.file, 'rb') as f:
            hasher = hashlib.sha1()
            hasher.update(f.read())
            return hasher.hexdigest()

    def is_cached(self):
        return os.path.exists(self.file)

    def has_hash(self):
        return self.hash

    def fetch(self):
        if self.is_cached():
            print '   + Using cached: %s'%self.file
        else:
            print '   + Fetching: %s'%self.url
            print '     Into: %s'%self.file
            urllib.urlretrieve(self.url, self.file)
        
    def validate(self):
        if self.has_hash():
            calculated_hash = self.calculate_hash()
            if calculated_hash != self.hash:
                print '    + Invalid checksum for %s: %s != %s'%(self.file, calculated_hash, self.hash)
                return False
            print '   + Checksum validated'
        return True

    def decompress(self, folder):
        print '   + Decompressing: %s'%self.file
        print '     Into: %s'%folder
        if not os.path.exists(folder):
            os.mkdir(folder)
        try:
            (opener, mode) = find_compressor(self.file)
            file = opener(self.file, mode)
            try: file.extractall(path=folder)
            finally: file.close()
            return True
        except Exception, e:
            print 'ERR  Failed to decompress %s: %s'%(self.file, e)
            return False

def fetch_commands(config, task_list, task):
    if not task_list:
        return []
    tags = []
    tags.append(config['architecture'])
    tags.append(config['linkage'])
    tags.append('%s-%s'%(config['architecture'], config['linkage']))
    result = []
    for t in tags:
        key = '%s-%s'%(task, t)
        if key in task_list:
            result.extend(task_list[key])
    if task in task_list:
        result.extend(task_list[task])
    return result

def render_command(config, command):
    for k, v in config.iteritems():
        command = command.replace('{{%s}}'%k, v)
    return command

def run_commands(config, tag, cmds):
    history = []
    try:
        with open('nscp.command.log', "r") as log:
            history = log.readlines()
    except:
        None
    with open('nscp.command.log', "a") as log:
        for cmd in cmds:
            cached_name = '%s: %s\n'%(tag, cmd)
            if cached_name in history:
                print '   + found cached command: %s (not running)'%tag
                continue
            rcmd = render_command(config, cmd)
            print '   + Executing: %s'%rcmd
            if msver >= '2012':
                rcmd = rcmd.replace('.vcproj', '.vcxproj')
            ret = os.system(rcmd)
            if ret != 0:
                print " ! Failed to execute: %s (%d)"%(rcmd, ret)
                return False
            log.write(cached_name)
            log.write('# %s\n'%rcmd)
    return True

def finalize_source(config, alias, source):
    if not 'url' in source:
        return None
    if not 'hash' in source:
        source['hash'] = None
    if not 'file' in source:
        source['file'] = source['url'].split('/')[-1]
    if source['file'].endswith('tar.gz'):
        folder = source['file'][0:-7]
        ext = 'tar.gz'
    else:
        (folder, ext) = os.path.splitext(source['file'])
    if not 'folder' in source:
        source['folder'] = folder
    if not 'ext' in source:
        source['ext'] = ext
    return source

def fetch_sources(config, alias, source):
    folder = config['target.folder']
    file = os.path.join(config['build.folder'], source['file'])
    if os.path.exists(folder):
        print '   + Found cached folder (assuming things are good)'
        return True
        
    s = source_helper(file, source['url'], source['hash'])
    s.fetch()
    if not s.validate():
        return False
    zFolder = get_root_from_file(file)
    if folder.endswith(zFolder):
        folder = config['build.folder']
    if not s.decompress(folder):
        return False
    return True

def write_config(config, target):
    data = {}
    for k, v in config.iteritems():
        data[k.replace('.', '_')] = v.replace('\\', '/')
    tpl = Template(CONFIG_TEMPLATE)
    result = tpl.safe_substitute(data)

    with open(os.path.join(target, 'build.cmake'), "w") as conf:
        conf.write(result)
    print ' + CMake config written to: %s'%target

boost_versions = {
    '2005': "msvc-8.0",
    '2012': "msvc-11.0_xp",
    '2013': "msvc-12.0_xp",
    '2014': "msvc-13.0_xp",
    '2015': "msvc-14.0_xp"
}
cmake_generator = { 
    '2005': "Visual Studio 8 2005", 
    '2012': "Visual Studio 11",
    '2013': "Visual Studio 12",
    '2014': "Visual Studio 13",
    '2015': "Visual Studio 14"
}

parser = OptionParser()
parser.add_option("-d", "--directory", help="Folder to build in (defaults to current)")
parser.add_option("-t", "--target", help="Which target architecture to build (win32 or x64)")
parser.add_option("-c", "--cmake-config", default="copy-target", help="Folder to place cmake configuration file in")
parser.add_option("-s", "--source", help="Location of the nscp source folder")
parser.add_option("-y", "--dyn", action="store_true", help="Use dynamic link")
parser.add_option("--msver", default='2012', help="Which version of visual studio you have")

(options, args) = parser.parse_args()

if not options.directory:
    options.directory = os.getcwd()
else:
    options.directory = os.path.abspath(options.directory)
if not os.path.exists(options.directory):
    os.makedirs(options.directory)
if not options.source:
    options.source = os.path.dirname(os.path.dirname(os.path.dirname(sys.argv[0])))

if options.source:
    options.source = os.path.abspath(options.source)

if not os.path.exists(options.source):
    print 'ERR  NSCP source folder was not found (strange, thats where this script should be right?) --source=%s'%options.source
    sys.exit(1)

msver = options.msver

data = json.loads(open('%s/build/python/requirements.json'%options.source).read())

config = {
    'version.msvc' : options.msver,
    'version.boost' : boost_versions[options.msver],
    'version.cmake' : cmake_generator[options.msver],
    'build.folder': options.directory,
    'python.folder': sys.prefix
}

cwd = os.getcwd()
for node, d in data.iteritems():
    print '****************************************************'
    print '*'
    print '* Building: %s'%node
    print '*'
    print '****************************************************'
    print ''
    config['name'] = node
    config['target.folder'] = os.path.join(options.directory, node)
    config['target.rel.folder'] = node
    config['source.folder'] = options.source
    config['architecture'] = 'x64' if options.target == 'x64' else 'w32'
    config['linkage'] = 'dynamic' if options.dyn else 'static'
    tasks = d['tasks'] if 'tasks' in d else None
    if 'source' in d:
        source = finalize_source(config, node, d['source'])
        config['target.folder'] = os.path.join(options.directory, source['folder'])
        config['target.rel.folder'] = source['folder']
    else:
        source = None
    config['%s.folder'%node] = config['target.folder']
    config['%s.rel.folder'%node] = config['target.rel.folder']
    for task in d['build']:
        print ' + Executing %s'%task
        if task == 'fetch':
            if not fetch_sources(config, node, source):
                print "FAILURE fetching source for: %s"%node
                os.chdir(cwd)
                sys.exit(1)
        elif task == 'mkdir':
            if not os.path.exists(config['target.folder']):
                os.mkdir(config['target.folder'])
        else:
            if not os.path.exists(config['target.folder']):
                print "FAILURE: No source for: %s"%node
                os.chdir(cwd)
                sys.exit(1)
            os.chdir(config['target.folder'])
            commands = fetch_commands(config, tasks, task)
            if not run_commands(config, task, commands):
                print "FAILURE"
                os.chdir(cwd)
                sys.exit(1)
    
os.chdir(cwd)

if options.cmake_config:
    if options.cmake_config == 'copy-target':
        options.cmake_config = config['build.folder']
    write_config(config, options.cmake_config)
else:
    print 'WARN Since you did not specify --cmake-config we will not write the cmake config file so most likely this was not very usefull...'
