from NSCP import Settings, Registry, Core, log, log_debug, log_error, status
import plugin_pb2
from optparse import OptionParser
from sets import Set
import os
import csv
import traceback
import StringIO
helper = None


def ttfix(string, def_val = ' '):
	if not string or len(string) == 0:
		return def_val
	return string
def key2link(str):
	str = str.replace('/', '_')
	str = str.replace(' ', '_')
	return str

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
	trac_path = None
	
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
		log_error('No keys found')
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

	def generate_trac_config_table(self, paths, module = None):
		found = False
		regular_keys = ''
		advanced_keys = ''
		for (p,pinfo) in paths.iteritems():
			if not module or module in pinfo.info.plugin:
				found = True
				found_key = False
				plink = '[wiki:%s/config#%s %s]'%(module, p.replace('/', '_'), p)
				for (k,kinfo) in pinfo.keys.iteritems():
					found_key = True
					link = '[wiki:%s/config#%s__%s %s]'%(module, key2link(p), key2link(k), k)
					if not kinfo.info.advanced:
						regular_keys += '||%s||%s ||%s ||%s \n'%(plink, link, kinfo.info.default_value, ttfix(kinfo.info.title))
					else:
						advanced_keys += '||%s||%s ||%s ||%s \n'%(plink, link, kinfo.info.default_value, ttfix(kinfo.info.title))
				if not found_key:
					regular_keys += '||%s|| || ||%s \n'%(plink, ttfix(pinfo.info.title))

		if not found:
			return False
		return "||= '''Path / Section''' =||= '''Key''' =||= '''Default value''' =||= '''Description'''\n%s%s"%(regular_keys, advanced_keys)

	def rst_table(self, table):
		cols = {}
		for line in table:
			i=0
			for col in line:
				if i in cols:
					cols[i] = max(cols[i], len(col))
				else:
					print col
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
		return '%s\n%s\n%s\n%s%s\n\n'%(divider, header, divider, data, divider)
		
	def any_value(self, value):
		if value.HasField("string_data"):
			return value.string_data
		return '??? %s ???'%value

	def rst_link(self, page, section):
		return ':ref:`%s <%s_%s>`'%(section, page.replace('/', '_'), section.replace('/', '_'))
		
	def rst_anchor(self, page, section):
		return '.. _%s_%s:\n\n'%(page.replace('/', '_'), section.replace('/', '_'))

	def generate_rst_config_table(self, paths, module = None):
		found = False
		regular_keys = []
		regular_keys.append(['Path / Section', 'Key', 'Default value', 'Description'])

		advanced_keys = []
		for (p,pinfo) in paths.iteritems():
			if not module or module in pinfo.info.plugin:
				found = True
				found_key = False
				plink = self.rst_link('%s/config'%module, '%s'%p)
				for (k,kinfo) in pinfo.keys.iteritems():
					found_key = True
					link = self.rst_link('%s/config'%module, '%s__%s'%(key2link(p), key2link(k)))
					if not kinfo.info.advanced:
						regular_keys.append([plink, link, self.any_value(kinfo.info.default_value), ttfix(kinfo.info.title)])
					else:
						advanced_keys.append([plink, link, self.any_value(kinfo.info.default_value), ttfix(kinfo.info.title)])
				if not found_key:
					regular_keys.append([plink, '', '', ttfix(pinfo.info.title)])
		if not found:
			return False
		regular_keys.extend(advanced_keys)
		return self.rst_table(regular_keys)

		#return "||= '''Path / Section''' =||= '''Key''' =||= '''Default value''' =||= '''Description'''\n%s%s"%(regular_keys, advanced_keys)
		
	def generate_trac_config_details(self, paths, table, module = None):
		string = ''
		string += '[[TracNav(TracNav/CC|nocollapse|noreorder)]]\n'
		string += '[[PageOutline]]\n'
		string += '= Configuration for %s module =\n'%module
		string += 'Section with configuration keys for the %s module\n\n'%module
		string += table + '\n\n'
		
		for (path,pinfo) in paths.iteritems():
			if not module or module in pinfo.info.plugin:
				string += '[=#%s]\n'%key2link(path)
				string += '== %s ==\n'%pinfo.info.title
				string += '%s\n\n'%(pinfo.info.description)
				string += "'''Section:''' %s\n\n"%(path)
				first = True
				for (k,kinfo) in pinfo.keys.iteritems():
					if not module or module in pinfo.info.plugin:
						if not kinfo.info.advanced:
							link = '[wiki:%s/config#%s__%s %s]'%(module, key2link(path), key2link(k), k)
							if first:
								string += "'''Keys:'''\n"
								string += "||= '''Key''' =||= '''Default''' =||= '''Title''' =||= '''Description'''\n"
								first = False
							string += '||%s||%s ||%s ||%s \n'%(link, kinfo.info.default_value, ttfix(kinfo.info.title), ttfix(kinfo.info.description, "'''TODO'''"))
				if not first:
					string += '\n\n'
				first = True
				for (k,kinfo) in pinfo.keys.iteritems():
					if not module or module in pinfo.info.plugin:
						if kinfo.info.advanced:
							link = '[wiki:%s/config#%s__%s %s]'%(module, key2link(path), key2link(k), k)
							if first:
								string += "'''Advanced Keys:'''\n"
								string += "||= '''Key''' =||= '''Default''' =||= '''Title''' =||= '''Description'''\n"
								first = False
							string += '||%s||%s ||%s ||%s \n'%(link, kinfo.info.default_value, ttfix(kinfo.info.title), ttfix(kinfo.info.description, "'''TODO'''"))
				if not first:
					string += '\n\n'
				string += "'''Sample:'''\n\n"
				string += "{{{\n"
				string += "# %s\n"%pinfo.info.title
				string += "# %s\n"%pinfo.info.description
				string += "[%s]\n"%path
				for (k,kinfo) in pinfo.keys.iteritems():
					if not module or module in pinfo.info.plugin:
						string += "# %s\n"%kinfo.info.title
						string += "# %s\n"%kinfo.info.description
						string += "%s=%s\n"%(k, kinfo.info.default_value)

				string += "}}}\n"
				string += '\n'
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

	def generate_trac_commands(self, command, cinfo, module = None):
		string = ""
		overview = ""
		try:
			overview += '[[TracNav(TracNav/CC|nocollapse|noreorder)]]\n'
			overview += '[[PageOutline]]\n'
			overview += "= %s =\n"%cinfo.info.title
			overview += "%s\n\n"%cinfo.info.description
			overview += "'''Provided by''': the [%s] module\n\n"%module
			overview += "'''Samples and usage''': This page provides reference information for samples and usage please see the samples page [wiki:%s/%s/samples].\n\n"%(module, command)

			string += '=== %s ===\n'%cinfo.info.title
			string += '%s\n\n'%(cinfo.info.description)
			string += "For details on this command go to the [wiki:%s/%s %s] page\n\n"%(module, command, command)

			(ret, msg, perf) = self.core.simple_query(command.encode('ascii', 'ignore'), ['help-csv'])
			if ret == 0:
				string += "'''Usage:''' (Click any option to go to the description page for that option)\n"
				reader = csv.reader(StringIO.StringIO(msg), delimiter=',')
				table = ""
				details = ""
				table += "||= '''Option''' =||= '''Default value''' =||= '''Description''' \n"
				for row in reader:
					if len(row) < 3:
						continue
					desc = row[3]
					ops = row[3].split("\\n", 1)
					if len(ops) == 1:
						ops += ['']
					(desc, rest) = ops
					if row[0] == 'help':
						link = '[wiki:%s/%s#%s_ %s]'%(module, command, row[0], row[0])
					else:
						link = '[wiki:%s/%s#%s %s]'%(module, command, row[0], row[0])
					if row[1] == "false":
						table += "|| %s ||N/A ||%s\n"%(link, desc)
					else:
						table += "|| %s ||%s ||%s\n"%(link, row[2], desc)
					
					if row[0] == 'help':
						details += "=== %s_ ===\n"%(row[0])
						details += "''The _ is due to trac bugs, real name is help''\n\n"
					else:
						details += "=== %s ===\n"%(row[0])
					details += "%s\n\n"%desc
					if rest:
						details += "'''Description''':\n%s\n\n"%rest.replace('\\n', '\n')
					if row[1] == "false":
						details += "'''Syntax''': %s\n\n"%(row[0])
						details += "'''Sample''':\n{{{\n%s ... %s ...\n}}}\n\n"%(command, row[0])
					else:
						if row[2] == "":
							details += "'''Syntax''': %s=ARGUMENT\n\n"%(row[0])
							details += "'''Sample''':\n{{{\n%s ... %s=ARGUMENT ...\n}}}\n\n"%(command, row[0])
						else:
							details += "'''Default value''': %s=%s\n\n"%(row[0], row[2])
							row[2] = row[2].replace('\"', '\\"')
							if ' ' in row[2]:
								details += "'''Sample''':\n{{{\n%s ... \"%s=%s\" ...\n}}}\n\n"%(command, row[0], row[2])
							else:
								details += "'''Sample''':\n{{{\n%s ... %s=%s ...\n}}}\n\n"%(command, row[0], row[2])
				string += table
				overview += table
				overview += '== Options ==\n'
				overview += details
				overview += "== Sample commands ==\n\n"
				overview += "Notice this section is included so please go [wiki:%s/%s/samples here] if you want to edit this section.\n\n"%(module, command)
				overview += "[[Include(wiki:%s/%s/samples)]]\n\n"%(module, command)
			else:
				overview = False
		except Exception as e:
			log_error('Failed to generate command details for: %s'%command)
			log_error(e)
			log_error('%s'%traceback.format_exc())
		return (string, overview)

	def rst_page_header(self):
		return ''

	def generate_rst_commands(self, command, cinfo, module = None):
		string = ""
		overview = ""
		try:
			overview += self.rst_page_header()
			overview += self.rst_anchor(module, command)
			overview += self.rst_title(0, cinfo.info.title)
			overview += self.rst_para(cinfo.info.description)
			overview += self.rst_para("Provided by", "the [%s] module"%module)
			overview += self.rst_para("Samples and usage", "This page provides reference information for samples and usage please see the samples page [wiki:%s/%s/samples]."%(module, command))

			string += self.rst_title(3, cinfo.info.title)
			string += self.rst_para(cinfo.info.description)
			string += self.rst_para("For details on this command go to the %s page"%self.rst_link(module, command))

			(ret, msg, perf) = self.core.simple_query(command.encode('ascii', 'ignore'), ['help-csv'])
			if ret == 0:
				string += self.rst_para("Usage", "(Click any option to go to the description page for that option)")
				reader = csv.reader(StringIO.StringIO(msg), delimiter=',')
				table = []
				details = ""
				table.append(['Option', 'Default value', 'Description'])
				for row in reader:
					if len(row) < 3:
						continue
					desc = row[3]
					ops = row[3].split("\\n", 1)
					if len(ops) == 1:
						ops += ['']
					(desc, rest) = ops
					link = self.rst_link('%s/%s'%(module, command), row[0])
					if row[1] == "false":
						table.append([link, 'N/A', desc])
					else:
						table.append([link, row[2], desc])
					
					details += self.rst_anchor('%s/%s'%(module, command), row[0])
					details += self.rst_title(2, row[0])
					details += self.rst_para(desc)
					if rest:
						details += self.rst_para("Description", "\n%s"%rest.replace('\\n', '\n'))
					if row[1] == "false":
						details += self.rst_para("Syntax", row[0])
						details += self.rst_para("Sample", "\n{{{\n%s ... %s ...\n}}}"%(command, row[0]))
					else:
						if row[2] == "":
							details += self.rst_para("Syntax", "%s=ARGUMENT"%(row[0]))
							details += self.rst_para("Sample", "\n{{{\n%s ... %s=ARGUMENT ...\n}}}"%(command, row[0]))
						else:
							details += self.rst_para("Default value", "%s=%s"%(row[0], row[2]))
							row[2] = row[2].replace('\"', '\\"')
							if ' ' in row[2]:
								details += self.rst_para("Sample", "\n{{{\n%s ... \"%s=%s\" ...\n}}}"%(command, row[0], row[2]))
							else:
								details += self.rst_para("Sample", "\n{{{\n%s ... %s=%s ...\n}}}"%(command, row[0], row[2]))
				table = self.rst_table(table)
				string += table
				overview += table
				overview += self.rst_title(1, 'Options')
				overview += details
				overview += self.rst_title(1, "Sample commands")
				overview += self.rst_para("Notice this section is included so please go [wiki:%s/%s/samples here] if you want to edit this section."%(module, command))
				overview += "[[Include(wiki:%s/%s/samples)]]\n\n"%(module, command)
			else:
				overview = False
		except Exception as e:
			log_error('Failed to generate command details for: %s'%command)
			log_error(e)
			log_error('%s'%traceback.format_exc())
		return (string, overview)		
		
	def serialize_wiki(self, string, filename, wikiname):
		if self.dir:
			if not os.path.exists(self.dir):
				os.makedirs(self.dir)
			f = open('%s/%s.wiki'%(self.dir, filename),"w")
			f.write(string)
			f.close()
			return "trac-admin %s wiki import %s %s.wiki\n"%(self.trac_path, wikiname, filename)
		else:
			print string
			return "Generated output for: %s"%wikiname

	def serialize_rst(self, string, path, filename, wikiname):
		if self.dir:
			if not os.path.exists(self.dir):
				os.makedirs(self.dir)
			tf = '%s/modules/%s'%(self.dir, path)
			if not os.path.exists(tf):
				os.makedirs(tf)
			f = open('%s/%s.rst'%(tf, filename),"w")
			f.write(string)
			f.close()
			return "%s/%s"%(path, filename)
		else:
			return "Generated output for: %s"%wikiname
	
	def generate_trac(self, dir, trac_path):
		self.dir = dir
		self.trac_path = trac_path
		if not trac_path:
			trac_path = '/foo/bar'
		docs = {}
		root = self.get_info()
		import_commands = ""
		
		i = 0
		for (module,minfo) in root.plugins.iteritems():
			i=i+1
			log_debug('Processing module: %d of %d [%s]'%(i, len(root.plugins), module))
			string = ''
			string += '[[TracNav(TracNav/CC|nocollapse|noreorder)]]\n'
			string += '[[PageOutline]]\n'
			string += "= %s =\n"%minfo.info.title
			string += "%s\n\n"%minfo.info.description
			string += '== Queries (Overview) ==\n'
			string += 'A list of all avalible queries (check commands)\n\n'
			query_found = False
			for (c,cinfo) in root.commands.iteritems():
				if module in cinfo.info.plugin:
					string += " * [[%s/%s|%s]]\n"%(module, cinfo.info.title, cinfo.info.title)
					string += "   %s\n"%cinfo.info.description
					query_found = True
			if not query_found:
				string += "No commands avalible in %s\n"%module
			else:
				string += '== Aliases ==\n'
				string += 'A list of all avalible aliases for queries and check commands\n\n'
				for (c,cinfo) in root.aliases.iteritems():
					if module in cinfo.info.plugin:
						if cinfo.info.description.startswith('Alternative name for:'):
							command = cinfo.info.description[22:]
							string += " * [[%s/%s|%s]]\n"%(module, command, cinfo.info.title)
							string += "   %s\n"%cinfo.info.description
							if command in root.commands:
								string += "   %s\n"%root.commands[command].info.description
						else:
							string += " * [[%s/%s|%s]]\n"%(module, cinfo.info.title, cinfo.info.title)
							string += "   %s\n"%cinfo.info.description

			string += "\n\n"
			string += '== Commands (executable) ==\n'
			string += "'''TODO:''' Add command list\n"
			string += "\n\n"
			#string += '[[PageOutline]]\n'
			string += '== Configuration ==\n'
			config_table = self.generate_trac_config_table(root.paths, module)
			if config_table:
				string += config_table
				import_commands += self.serialize_wiki(self.generate_trac_config_details(root.paths, config_table, module), '%s_config'%module, '%s/config'%module)
			else:
				string += "''No configuration avalible for %s''\n\n"%module
				
			if query_found:
				string += '== Queries (Reference) ==\n'
				string += 'A quick reference for all avalible queries (check commands) in the %s module.\n\n'%module
				for (c,cinfo) in root.commands.iteritems():
					if module in cinfo.info.plugin:
						(overview, details) =  self.generate_trac_commands(c, cinfo, module)
						string += overview
						if details:
							import_commands += self.serialize_wiki(details, '%s_%s'%(module, c), '%s/%s'%(module, c))

			import_commands += self.serialize_wiki(string, module, module)

		all_config = """
[[TracNav(TracNav/TOCDoc|nocollapse|noreorder)]]
[[PageOutline]]
= Configuration 0.4.x =

For older version please refer to [[wiki:/doc/configuration/0.3.x]]

Configuration is fairly simple and straight forward. But due to flexibility the actual file may be placed in many location and can even not be a file at all (for instance the registry). Regardless of which store you have for configuration the end result is the same. The configuration will const of a section (path) a key and a value. The path (section) is a hierarcical structure meaning you will find things like ''/foo/bar/baz'' or to make real examples ''/settings/NRPE/server''. This is similar to how it was in older version except there we would only have had NRPE in the path (section) name.
If your configuration is in a file (and most likely it is) you can edit it using a normal text editor (such as notepad or vi). The file is usually called nsc.ini or nsclient.ini (but this can be changed).

The configuration is as mentioned divided into sections (paths) each with a given name.

The various sections are described in short below. The default configuration file has a lot of examples and comments so make sure you change this before you use NSClient++ as some of the examples might be potential security issues.
		"""
		all_config = self.generate_trac_config_table(root.paths)
		#for (p,pinfo) in root.paths.iteritems():
		#	all_config += self.generate_trac_config_path(p, pinfo)
		import_commands += self.serialize_wiki(all_config, "all-config", "doc/configuration/0.4.x")
			
			
		all_commands = """
[[TracNav(TracNav/CC|nocollapse)]]
[[PageOutline]]
= Modules =
NSClient++ comes with a set of modules out of the box that perform various checks and functions. A list of the modules and their potential use is listed below.  Click each plug-in to see detailed command descriptions and how the various modules can be used.
"""
		for (module,minfo) in root.plugins.iteritems():
			all_commands += "== [[wiki:%s]] ==\n"%(module)
			all_commands += "'''%s''': %s\n\n"%(minfo.info.title, minfo.info.description)
			all_commands += '=== Queries (commands) ===\n'
			found = False
			for (c,cinfo) in root.commands.iteritems():
				if module in cinfo.info.plugin:
					all_commands += " * [[%s/%s|%s]]\n"%(module, cinfo.info.title, cinfo.info.title)
					all_commands += "   %s\n"%cinfo.info.description
					found = True
			if not found:
				all_commands += "No commands avalible in [[%s]]\n"%module
			
			all_commands += "\n\n"
			all_commands += '=== Commands (executable) ===\n'
			all_commands += "'''TODO:''' Add command list\n"
			all_commands += "\n\n"
		all_commands += """
= All Commands =
A list of all commands (alphabetically).
[[ListTagged(check)]]
"""
		import_commands += self.serialize_wiki(all_commands, "all-commands", "CheckCommands")
		#log_error('-------------------------------------------------------------')
		#log_error(import_commands)
		#log_error('-------------------------------------------------------------')
		print import_commands
		#log_error('-------------------------------------------------------------')

		
	def rst_title(self, level, title):
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

	def rst_para(self, text, text2 = None):
		if text2:
			return '%s: %s\n\n'%(text, text2)
		return '%s\n\n'%text
	
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
			
	def generate_rst(self, dir, trac_path):
		self.dir = dir
		self.trac_path = trac_path
		if not trac_path:
			trac_path = '/foo/bar'
		docs = {}
		root = self.get_info()
		import_commands = []
		import_config = ""
		
		i = 0
		for (module,minfo) in root.plugins.iteritems():
			i=i+1
			log_debug('Processing module: %d of %d [%s]'%(i, len(root.plugins), module))
			string = ''
			string += self.rst_title(0, minfo.info.title)
			string += self.rst_para(minfo.info.description)
			string += self.rst_title(1, 'Queries (Overview)')
			string += self.rst_para('A list of all avalible queries (check commands)')
			string += ".. toctree::\n"
			string += "   :maxdepth: 2\n"
			string += "   \n"

			query_found = False
			for (c,cinfo) in root.commands.iteritems():
				if module in cinfo.info.plugin:
					string += "   %s.rst\n"%cinfo.info.title
					query_found = True
			if not query_found:
				string += self.rst_para("No commands avalible in %s"%module)
			else:
				string += "   \n"
				string += self.rst_title(1, 'Aliases')
				string += self.rst_para('A list of all avalible aliases for queries and check commands')
				for (c,cinfo) in root.aliases.iteritems():
					if module in cinfo.info.plugin:
						if cinfo.info.description.startswith('Alternative name for:'):
							command = cinfo.info.description[22:]
							string += " * %s\n"%self.rst_link('%s/%s'%(module, command), cinfo.info.title)
							string += "   %s\n"%cinfo.info.description
							if command in root.commands:
								string += "   %s\n"%root.commands[command].info.description
						else:
							string += " * %s\n"%self.rst_link('%s/%s'%(module, command), cinfo.info.title)
							string += "   %s\n"%cinfo.info.description

			string += self.rst_para('')
			string += self.rst_title(1, 'Commands (executable)')
			string += self.rst_para("'''TODO:''' Add command list")
			#string += '[[PageOutline]]\n'
			string += self.rst_title(1, 'Configuration')
			config_table = self.generate_rst_config_table(root.paths, module)
			if config_table:
				string += config_table
				import_commands.append(self.serialize_rst(self.generate_trac_config_details(root.paths, config_table, module), module, 'config', '%s/config'%module))
			else:
				string += self.rst_para("''No configuration avalible for %s''"%module)
				
			if query_found:
				string += self.rst_title(1, 'Queries (Reference)')
				string += self.rst_para('A quick reference for all avalible queries (check commands) in the %s module.'%module)
				for (c,cinfo) in root.commands.iteritems():
					if module in cinfo.info.plugin:
						(overview, details) =  self.generate_rst_commands(c, cinfo, module)
						string += overview
						if details:
							import_commands.append(self.serialize_rst(details, module, c, '%s/%s'%(module, c)))

			import_commands.append(self.serialize_rst(string, module, 'index', module))

		all_config = self.generate_rst_config_table(root.paths)
		import_config += self.serialize_rst(all_config, '', "all-config", "doc/configuration/0.4.x")
			
			
		all_commands = """
[[TracNav(TracNav/CC|nocollapse)]]
[[PageOutline]]
= Modules =
NSClient++ comes with a set of modules out of the box that perform various checks and functions. A list of the modules and their potential use is listed below.  Click each plug-in to see detailed command descriptions and how the various modules can be used.
"""
		for (module,minfo) in root.plugins.iteritems():
			all_commands += self.rst_title(1, module)
			all_commands += self.rst_para(minfo.info.title, minfo.info.description)
			all_commands += self.rst_title(2,'Queries (commands)')
			found = False
			for (c,cinfo) in root.commands.iteritems():
				if module in cinfo.info.plugin:
					all_commands += " * %s\n"%self.rst_link('%s/%s'%(module, command), cinfo.info.title)
					#all_commands += " * [[%s/%s|%s]]\n"%(module, cinfo.info.title, cinfo.info.title)
					all_commands += "   %s\n"%cinfo.info.description
					found = True
			if not found:
				all_commands += self.rst_para("No commands avalible in [[%s]]"%module)
			
			all_commands += self.rst_title(2,'Commands (executable)')
			all_commands += self.rst_para("TODO", 'Add command list')
		all_commands += """
= All Commands =
A list of all commands (alphabetically).
[[ListTagged(check)]]
"""
		import_commands.append(self.serialize_wiki(all_commands, "all-commands", "CheckCommands"))
		#log_error('-------------------------------------------------------------')
		#log_error(import_commands)
		#log_error('-------------------------------------------------------------')
		self.rst_build_command(import_commands)
		#print import_commands
		#log_error('-------------------------------------------------------------')

	def main(self, args):
		parser = OptionParser(prog="")
		parser.add_option("-f", "--format", help="Generate format")
		parser.add_option("-o", "--output", help="write report to FILE(s)")
		parser.add_option("--trac-path", help="The path to track (used for importing wikis)")
		(options, args) = parser.parse_args(args=args)

		if options.format in ["trac"]:
			self.generate_trac(options.output, options.trac_path)
		if options.format in ["rst"]:
			self.generate_rst(options.output, options.trac_path)
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


