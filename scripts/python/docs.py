from NSCP import Settings, Registry, Core, log, log_debug, log_error, status
import plugin_pb2
from optparse import OptionParser
from sets import Set
import os
import csv
import traceback
import StringIO
import string
helper = None


def ttfix(string, def_val = ' '):
	if not string or len(string) == 0:
		return def_val
	return string
def key2link(str):
	str = str.replace('/', '_')
	str = str.replace(' ', '_')
	return str
	
def indent(numSpaces, s):
	ret = ''
	for line in s.split('\n'):
		if line != '\n':
			ret += (numSpaces * ' ') + line + '\n'
		else:
			ret += line + '\n'
	return ret
	#str = '\n'.join([(numSpaces * ' ') + string.lstrip(line) for line in s.split('\n')])

class RSTRenderer(object):
	padding = 0
	
	def __init__(self):
		self.padding = 0
	
	def page_header(self, type, key):
		return '.. nscp:%s:: %s\n\n'%(type, key)

	def table(self, table):
		cols = {}
		for line in table:
			i=0
			for col in line:
				if i in cols:
					cols[i] = max(cols[i], len(col))
				else:
					cols[i] = len(col)
				i=i+1
		divider = ''
		header = ''
		data = ''
		first = True
		for line in table:
			i=0
			for col in line:
				if first:
					divider += '%s '%''.ljust(cols[i]+1, '=')
					header += '%s '%col.ljust(cols[i]+1, ' ')
				else:
					data += '%s '%col.ljust(cols[i]+1, ' ')
				i=i+1
			if not first:
				data += "\n"
			first = False
		return indent(self.padding, '%s\n%s\n%s\n%s%s\n\n'%(divider, header, divider, data, divider))
		
	def obj_link(self, type, name):
		return ':ref:nscp:%s:`%s`'%(type, name.replace('/', '_'))

	def obj_anchor(self, type, name, desc):
		return '.. nscp:%s:: %s\n    :synopsis: %s\n\n'%(type, name.replace('/', '_'), desc)
		
	def link(self, k1, k2=None, k3=None, k4=None, title=None):
		keys = k1.replace('/', '_')
		if k2:
			keys += "__" + k2.replace('/', '_')
		if k3:
			keys += "__" + k3.replace('/', '_')
		if k4:
			keys += "__" + k4.replace('/', '_')
		if title:
			return ':ref:`%s <%s>`'%(title, keys)
		return ':ref:`%s`'%(keys)
		
	def anchor(self, k1, k2=None, k3=None, k4=None):
		keys = k1.replace('/', '_')
		if k2:
			keys += "__" + k2.replace('/', '_')
		if k3:
			keys += "__" + k3.replace('/', '_')
		if k4:
			keys += "__" + k4.replace('/', '_')
		return '.. _%s:\n\n'%keys
		
	def title(self, level, title):
		l = len(title)
		if level == 0:
			tag = ''.rjust(l, '=')
			return "%s\n%s\n%s\n"%(tag, title, tag)
		else:
			if level == 1:
				char = '='
			elif level == 2:
				char = '-'
			else:
				char = '.'
			tag = ''.rjust(l, char)
			return "%s\n%s\n"%(title, tag)

	def para(self, text, text2 = None):
		if text2:
			return indent(self.padding, '%s: %s\n\n'%(text, text2))
		return indent(self.padding, '%s\n\n'%text)
		
	def serialize(self, string, filename):
		path = os.path.dirname(filename)
		if not os.path.exists(path):
			os.makedirs(path)
		f = open(filename,"w")
		f.write(string)
		f.close()
		log_debug('Writing file: %s'%filename)
		
	def indent(self, chars):
		self.padding += chars
		
	def sample(self, block, language, pad=4):
		return '**Sample**:\n\n%s\n\n'%self.code_block(block, language, pad)
		
	def code_block(self, block, language, padlen=4):
		return '.. codeblock::%s\n\n%s\n\n'%(language, indent(padlen, block))

class root_container(object):
	paths = {}
	commands = {}
	aliases = {}
	plugins = {}
	def __init__(self):
		self.paths = {}
		self.commands = {}
		self.aliases = {}
		self.plugins = {}

	def append_key(self, info):
		path = info.node.path
		if path in self.paths:
			self.paths[path].append_key(info)
		else:
			p = path_container()
			p.append_key(info)
			self.paths[path] = p
		
	def append_path(self, info):
		path = info.node.path
		if not path in self.paths:
			self.paths[path] = path_container(info)

	def append_command(self, info):
		name = info.name
		if not name in self.commands:
			self.commands[name] = command_container(info)
	def append_alias(self, info):
		name = info.name
		if not name in self.commands:
			self.aliases[name] = command_container(info)

	def append_plugin(self, info):
		name = info.name
		if not name in self.plugins:
			self.plugins[name] = plugin_container(info)

class path_container(object):
	keys = {}
	info = None
	def __init__(self, info = None):
		self.keys = {}
		self.info = info.info
		
	def append_key(self, info):
		self.keys[info.node.key] = info

class command_container(object):
	info = None
	def __init__(self, info = None):
		self.info = info.info

class plugin_container(object):
	info = None
	def __init__(self, info = None):
		self.info = info.info

class DocumentationHelper(object):
	plugin_id = None
	plugin_alias = None
	script_alias = None
	conf = None
	registry = None
	core = None
	dir = None
	renderer = RSTRenderer()
	
	def __init__(self, plugin_id, plugin_alias, script_alias):
		self.plugin_id = plugin_id
		self.plugin_alias = plugin_alias
		self.script_alias = script_alias
		self.conf = Settings.get(self.plugin_id)
		self.registry = Registry.get(self.plugin_id)
		self.core = Core.get(self.plugin_id)
		
	def build_inventory_request(self,  path = '/', recursive = True, keys = False):
		message = plugin_pb2.SettingsRequestMessage()
		message.header.version = plugin_pb2.Common.VERSION_1
		payload = message.payload.add()
		payload.plugin_id = self.plugin_id
		payload.inventory.node.path = path
		payload.inventory.recursive_fetch = recursive
		payload.inventory.fetch_keys = keys
		payload.inventory.fetch_paths = not keys
		payload.inventory.fetch_samples = True
		payload.inventory.descriptions = True
		return message.SerializeToString()
	
	def build_command_request(self, type = 1):
		message = plugin_pb2.RegistryRequestMessage()
		message.header.version = plugin_pb2.Common.VERSION_1
		payload = message.payload.add()
		payload.inventory.fetch_all = True
		payload.inventory.type.append(type)
		return message.SerializeToString()

	def get_paths(self):
		(code, data) = self.conf.query(self.build_inventory_request())
		if code == 1:
			message = plugin_pb2.SettingsResponseMessage()
			message.ParseFromString(data)
			for payload in message.payload:
				if payload.inventory:
					log_debug('Found %d paths'%len(payload.inventory))
					return payload.inventory
		return []

	def get_keys(self, path):
		(code, data) = self.conf.query(self.build_inventory_request(path, False, True))
		if code == 1:
			message = plugin_pb2.SettingsResponseMessage()
			message.ParseFromString(data)
			for payload in message.payload:
				if payload.inventory:
					log_debug('Found %d keys for %s'%(len(payload.inventory), path))
					return payload.inventory
		return []

	def get_queries(self):
		(code, data) = self.registry.query(self.build_command_request(1))
		if code == 1:
			message = plugin_pb2.RegistryResponseMessage()
			message.ParseFromString(data)
			for payload in message.payload:
				if payload.inventory:
					log_debug('Found %d commands'%len(payload.inventory))
					return payload.inventory
		log_error('No commands found')
		return []
	def get_query_aliases(self):
		(code, data) = self.registry.query(self.build_command_request(5))
		if code == 1:
			message = plugin_pb2.RegistryResponseMessage()
			message.ParseFromString(data)
			for payload in message.payload:
				if payload.inventory:
					log_debug('Found %d commands'%len(payload.inventory))
					return payload.inventory
		log_error('No commands found')
		return []

	def get_plugins(self):
		(code, data) = self.registry.query(self.build_command_request(4))
		if code == 1:
			message = plugin_pb2.RegistryResponseMessage()
			message.ParseFromString(data)
			for payload in message.payload:
				if payload.inventory:
					log_debug('Found %d plugins'%len(payload.inventory))
					return payload.inventory
		log_error('No plugins')
		return []

	def get_info(self):
		root = root_container()
		for p in self.get_paths():
			root.append_path(p)
			for k in self.get_keys(p.node.path):
				root.append_key(k)
		for p in self.get_queries():
			root.append_command(p)
		for p in self.get_query_aliases():
			root.append_alias(p)
		for p in self.get_plugins():
			root.append_plugin(p)
		return root


	def any_value(self, value):
		if value.HasField("string_data"):
			return value.string_data
		return '??? %s ???'%value


	def generate_rst_config_table(self, paths, module = None):
		renderer = self.renderer
		regular_keys = []

		advanced_keys = []
		sample_keys = []
		for (p,pinfo) in sorted(paths.iteritems()):
			if not module or module in pinfo.info.plugin:
				found_key = False
				plink = renderer.link('%s/config'%module, '%s'%p)
				for (k,kinfo) in pinfo.keys.iteritems():
					found_key = True
					link = renderer.link('%s/config'%module, '%s__%s'%(key2link(p), key2link(k)))
					if kinfo.info.sample:
						sample_keys.append([plink, link, self.any_value(kinfo.info.default_value), ttfix(kinfo.info.title)])
					elif not kinfo.info.advanced:
						regular_keys.append([plink, link, self.any_value(kinfo.info.default_value), ttfix(kinfo.info.title)])
					else:
						advanced_keys.append([plink, link, self.any_value(kinfo.info.default_value), ttfix(kinfo.info.title)])
				if not found_key:
					regular_keys.append([plink, '', '', ttfix(pinfo.info.title)])
		if regular_keys or advanced_keys or sample_keys:
			regular_keys.insert(0, ['Path / Section', 'Key', 'Default value', 'Description'])
			ret = renderer.table(regular_keys)
			if advanced_keys:
				ret += 'Advanced keys:\n\n'
				ret += renderer.table(advanced_keys)
			if sample_keys:
				ret += 'Sample keys:\n\n'
				ret += renderer.table(sample_keys)
			return ret
		return False

	def generate_rst_config_details(self, paths, module = None):
		renderer = self.renderer
		string = ''
		
		for (path,pinfo) in paths.iteritems():
			if not module or module in pinfo.info.plugin:
				string += renderer.title(3, pinfo.info.title)
				string += renderer.para(pinfo.info.description)

				regular_keys = []
				advanced_keys = []
				sample_keys = []
				for (k,kinfo) in sorted(pinfo.keys.iteritems()):
					link = renderer.link('%s/config'%module, '%s__%s'%(key2link(path), key2link(k)))
					if kinfo.info.sample:
						sample_keys.append([link, self.any_value(kinfo.info.default_value), ttfix(kinfo.info.title)])
					elif not kinfo.info.advanced:
						regular_keys.append([link, self.any_value(kinfo.info.default_value), ttfix(kinfo.info.title)])
					else:
						advanced_keys.append([link, self.any_value(kinfo.info.default_value), ttfix(kinfo.info.title)])
				if regular_keys or advanced_keys or sample_keys:
					regular_keys.insert(0, ['Key', 'Default Value', 'Description'])
					regular_keys.extend(advanced_keys)
					regular_keys.extend(sample_keys)
					string += renderer.table(regular_keys)
				
				sample  = "    # %s\n"%pinfo.info.title
				sample += "    # %s\n"%pinfo.info.description
				sample += "    [%s]\n"%path
				for (k,kinfo) in pinfo.keys.iteritems():
					if not module or module in pinfo.info.plugin:
						sample += "    # %s\n"%kinfo.info.title
						sample += "    # %s\n"%kinfo.info.description
						sample += "    %s=%s\n"%(k, kinfo.info.default_value)
				string += renderer.sample(sample, 'ini')

				for (k,kinfo) in pinfo.keys.iteritems():
					if not module or module in pinfo.info.plugin:
						string += '[=#%s__%s]\n'%(key2link(path), key2link(k))
						string += "=== %s ===\n"%kinfo.info.title
						string += "'''Description:''' %s\n\n"%kinfo.info.description
						if kinfo.info.advanced:
							string += "'''Advanced:''' (means it is not commonly used)\n\n"
						string += "'''Key:''' %s\n\n"%k
						if kinfo.info.default_value:
							string += "'''Default value:''' %s\n\n"%kinfo.info.default_value
						string += "'''Used by:''' "
						first = True
						for p in pinfo.info.plugin:
							if first:
								first = False
							else:
								string += ", "
							string += "[[%s]]"%p
						string += "\n\n"
						string += "'''Sample:'''\n\n"
						string += "{{{\n"
						string += "# %s\n"%kinfo.info.title
						string += "# %s\n"%kinfo.info.description
						string += "[%s]\n"%path
						string += "%s=%s\n"%(k, kinfo.info.default_value)
						string += "}}}\n"
						string += '\n'
		return string

	def generate_rst_command_details(self, command, cinfo, module):
		renderer = self.renderer
		string = ""
		try:
			string += renderer.obj_anchor('query', command, cinfo.info.description)
			renderer.indent(4)
			string += renderer.para(cinfo.info.description)

			(ret, msg, perf) = self.core.simple_query(command.encode('ascii', 'ignore'), ['help-csv'])
			if ret == 0:
				string += renderer.para("Usage", "(Click any option to go to the description page for that option)")
				reader = csv.reader(StringIO.StringIO(msg), delimiter=',')
				table = []
				details = ""
				for row in reader:
					if len(row) < 3:
						continue
					renderer.indent(4)
					desc = row[3]
					ops = row[3].split("\\n", 1)
					if len(ops) == 1:
						ops += ['']
					(desc, rest) = ops
					link = renderer.link('%s/%s'%(module, command), row[0])
					if row[1] == "false":
						table.append([link, 'N/A', desc])
					else:
						table.append([link, row[2], desc])
					
					details += renderer.anchor('%s/%s'%(module, command), row[0])
					details += renderer.title(2, row[0])
					details += renderer.para(desc)
					if rest:
						details += renderer.para("Description", "\n%s"%rest.replace('\\n', '\n'))
					if row[1] == "false":
						details += renderer.para("Syntax", row[0])
						details += renderer.para("Sample", "\n{{{\n%s ... %s ...\n}}}"%(command, row[0]))
					else:
						if row[2] == "":
							details += renderer.para("Syntax", "%s=ARGUMENT"%(row[0]))
							details += renderer.para("Sample", "\n{{{\n%s ... %s=ARGUMENT ...\n}}}"%(command, row[0]))
						else:
							details += renderer.para("Default value", "%s=%s"%(row[0], row[2]))
							row[2] = row[2].replace('\"', '\\"')
							if ' ' in row[2]:
								details += renderer.para("Sample", "\n{{{\n%s ... \"%s=%s\" ...\n}}}"%(command, row[0], row[2]))
							else:
								details += renderer.para("Sample", "\n{{{\n%s ... %s=%s ...\n}}}"%(command, row[0], row[2]))
					renderer.indent(-4)
				if table:
					table.insert(0, ['Option', 'Default value', 'Description'])
					string += renderer.table(table)
					string += details
			renderer.indent(-4)
		except Exception as e:
			return '%s'%e
		return string
		
	def rst_build_command(self, commands):
		ret = """Modules
=======

Contents:

.. toctree::
   :maxdepth: 3

"""
		for c in commands:
			if 'index' in c:
				ret += '   %s.rst\n'%c
		self.serialize_rst(ret, '', 'index', 'TODO')
			
	def generate_rst(self, dir):
		renderer = self.renderer
		docs = {}
		root = self.get_info()
		import_commands = []
		import_config = ""
		
		i = 0
		for (module,minfo) in root.plugins.iteritems():
			i=i+1
			log_debug('Processing module: %d of %d [%s]'%(i, len(root.plugins), module))
			string = renderer.page_header('module', module)
			string += renderer.title(0, minfo.info.title)
			string += renderer.para(minfo.info.description)
			string += renderer.title(1, 'Queries (Overview)')
			string += renderer.para('A list of all avalible queries (check commands)')

			queries = []
			for (c,cinfo) in sorted(root.commands.iteritems()):
				if module in cinfo.info.plugin:
					queries.append([renderer.obj_link('query', c), cinfo.info.description.split('\n')[0]])
			if not queries:
				string += renderer.para("No commands avalible in %s"%module)
			else:
				queries.insert(0, ['Command', 'Description'])
				string += renderer.table(queries)
				
			table = []
			for (c,cinfo) in sorted(root.aliases.iteritems()):
				if module in cinfo.info.plugin:
					if cinfo.info.description.startswith('Alternative name for:'):
						command = cinfo.info.description[22:]
						table.append([c, ':ref:nscp:query:`%s`'%command, cinfo.info.title])
					else:
						table.append([c, '', cinfo.info.title])
			if table:
				table.insert(0, ['Alias', 'Command', 'Description'])
				string += renderer.table(table)

			string += renderer.title(1, 'Commands (executable)')
			string += renderer.para("**TODO:** Add command list")

			string += renderer.title(1, 'Configuration')
			config_table = self.generate_rst_config_table(root.paths, module)
			if config_table:
				string += config_table
			else:
				string += renderer.para("''No configuration avalible for %s''"%module)
				
			if queries:
				string += renderer.title(1, 'Queries (Reference)')
				string += renderer.para('A quick reference for all avalible queries (check commands) in the %s module.'%module)
				for (c,cinfo) in root.commands.iteritems():
					if module in cinfo.info.plugin:
						string += self.generate_rst_command_details(c, cinfo, module)

			if config_table:
				string += renderer.title(1, 'Configuration (Reference)')
				string += renderer.para('A quick reference for all avalible configuration options in the %s module.'%module)
				string += self.generate_rst_config_details(root.paths, module)
			
			renderer.serialize(string, '%s/reference/%s.rst'%(dir, module))

		all_config = self.generate_rst_config_table(root.paths)
		renderer.serialize(all_config, '%s/reference/config.rst'%dir)
			
			

		#renderer.build_command(import_commands)

	def main(self, args):
		parser = OptionParser(prog="")
		parser.add_option("-f", "--format", help="Generate format")
		parser.add_option("-o", "--output", help="write report to FILE(s)")
		parser.add_option("--trac-path", help="The path to track (used for importing wikis)")
		(options, args) = parser.parse_args(args=args)

		#if options.format in ["trac"]:
		#	self.generate_trac(options.output, options.trac_path)
		if options.format in ["rst"]:
			self.generate_rst(options.output)
		else:
			log("Help%s"%parser.print_help())
			log("Invalid format: %s"%options.format)
			return

def __main__(args):
	global helper
	helper.main(args);
	
def init(plugin_id, plugin_alias, script_alias):
	global helper
	helper = DocumentationHelper(plugin_id, plugin_alias, script_alias)

def shutdown():
	global helper
	helper = None


