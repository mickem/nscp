from NSCP import Settings, Registry, Core, log, log_debug, log_error, status
import plugin_pb2
from optparse import OptionParser
from sets import Set
import os
helper = None


def ttfix(string, def_val = ' '):
	if not string or len(string) == 0:
		return def_val
	return string

class root_container(object):
	paths = {}
	commands = {}
	plugins = {}
	def __init__(self):
		self.paths = {}
		self.commands = {}
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
	dir = None
	trac_path = None
	
	def __init__(self, plugin_id, plugin_alias, script_alias):
		self.plugin_id = plugin_id
		self.plugin_alias = plugin_alias
		self.script_alias = script_alias
		self.conf = Settings.get(self.plugin_id)
		self.registry = Registry.get(self.plugin_id)
		
	def build_inventory_request(self, path = '/', recursive = True, keys = False):
		message = plugin_pb2.SettingsRequestMessage()
		message.header.version = plugin_pb2.Common.VERSION_1
		payload = message.payload.add()
		payload.type = 4
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
		payload.type = 2
		payload.inventory.fetch_all = True
		payload.inventory.type.append(type)
		return message.SerializeToString()

	def get_paths(self):
		(code, data) = self.conf.query(self.build_inventory_request())
		if code == 1:
			message = plugin_pb2.SettingsResponseMessage()
			message.ParseFromString(data)
			for payload in message.payload:
				if payload.type == 4:
					return payload.inventory
		return []

	def get_keys(self, path):
		(code, data) = self.conf.query(self.build_inventory_request(path, False, True))
		if code == 1:
			message = plugin_pb2.SettingsResponseMessage()
			message.ParseFromString(data)
			for payload in message.payload:
				if payload.type == 4:
					return payload.inventory
		return []

	def get_commands(self):
		(code, data) = self.registry.query(self.build_command_request(1))
		if code == 1:
			message = plugin_pb2.RegistryResponseMessage()
			message.ParseFromString(data)
			for payload in message.payload:
				if payload.type == 2:
					return payload.inventory
		return []

	def get_plugins(self):
		(code, data) = self.registry.query(self.build_command_request(4))
		if code == 1:
			message = plugin_pb2.RegistryResponseMessage()
			message.ParseFromString(data)
			for payload in message.payload:
				if payload.type == 2:
					return payload.inventory
		return []

	def get_info(self):
		root = root_container()
		for p in self.get_paths():
			root.append_path(p)
			for k in self.get_keys(p.node.path):
				root.append_key(k)
		for p in self.get_commands():
			root.append_command(p)
		for p in self.get_plugins():
			root.append_plugin(p)
		return root
		
	def generate_trac_config_path(self, p, pinfo, module = None):
		string = ''
		if not module or module in pinfo.info.plugin:
			string += '=== %s ===\n'%pinfo.info.title
			string += '%s\n\n'%(pinfo.info.description)
			string += "'''Section:''' %s\n\n"%(p)
			first = True
			for (k,kinfo) in pinfo.keys.iteritems():
				if not module or module in pinfo.info.plugin:
					if not kinfo.info.advanced:
						if first:
							string += "'''Keys:'''\n"
							string += "||'''Key'''||'''Title'''||'''Description'''\n"
							first = False
						string += '||%s||%s ||%s \n'%(ttfix(k), ttfix(kinfo.info.title), ttfix(kinfo.info.description))
			if not first:
				string += '\n\n'
			first = True
			for (k,kinfo) in pinfo.keys.iteritems():
				if not module or module in pinfo.info.plugin:
					if kinfo.info.advanced:
						if first:
							string += "'''Advanced Keys:'''\n"
							string += "||'''Key'''||'''Title'''||'''Description'''\n"
							first = False
						string += '||%s||%s ||%s \n'%(ttfix(k), ttfix(kinfo.info.title), ttfix(kinfo.info.description, "'''TODO'''"))
			if not first:
				string += '\n\n'
			string += "'''Sample:'''\n\n"
			string += "{{{\n"
			string += "# %s\n"%pinfo.info.title
			string += "# %s\n"%pinfo.info.description
			string += "[%s]\n"%p
			for (k,kinfo) in pinfo.keys.iteritems():
				if not module or module in pinfo.info.plugin:
					string += "# %s\n"%kinfo.info.title
					string += "# %s\n"%kinfo.info.description
					string += "%s=%s\n"%(k, kinfo.info.default_value)

			string += "}}}\n"
			string += '\n'
			for (k,kinfo) in pinfo.keys.iteritems():
				if not module or module in pinfo.info.plugin:
					string += "==== %s ====\n"%kinfo.info.title
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
					string += "[%s]\n"%p
					string += "%s=%s\n"%(k, kinfo.info.default_value)
					string += "}}}\n"
					string += '\n'
		return string

		
	def serialize_wiki(self, string, filename, wikiname):
		if dir:
			if not os.path.exists(self.dir):
				os.makedirs(self.dir)
			f = open('%s/%s.wiki'%(self.dir, filename),"w")
			f.write(string)
			f.close()
			return "trac-admin %s wiki import %s %s.wiki\n"%(self.trac_path, wikiname, filename)
		else:
			print string
			return "Generated output for: %s"%wikiname
	
	def generate_trac(self, dir, trac_path):
		self.dir = dir
		self.trac_path = trac_path
		if not trac_path:
			trac_path = '/foo/bar'
		docs = {}
		root = self.get_info()
		import_commands = ""
		
		for (module,minfo) in root.plugins.iteritems():
			string = ''
			string += '[[TracNav(TracNav/CC|nocollapse|noreorder)]]\n'
			string += '[[PageOutline]]\n'
			string += "= %s =\n"%minfo.info.title
			string += "%s\n\n"%minfo.info.description
			string += '== Queries (commands) ==\n'
			found = False
			for (c,cinfo) in root.commands.iteritems():
				if module in cinfo.info.plugin:
					string += " * [[%s/%s|%s]]\n"%(module, cinfo.info.title, cinfo.info.title)
					string += "   %s\n"%cinfo.info.description
					found = True
			if not found:
				string += "No commands avalible in %s\n"%module
			
			string += "\n\n"
			string += '== Commands (executable) ==\n'
			string += "'''TODO:''' Add command list\n"
			string += "\n\n"
			string += '== Configuration ==\n'
			found = None
			for (p,pinfo) in root.paths.iteritems():
				found = self.generate_trac_config_path(p, pinfo, module)
				string += found

			if not found:
				string += "No configuration avalible for %s\n"%module
				
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
		for (p,pinfo) in root.paths.iteritems():
			all_config += self.generate_trac_config_path(p, pinfo)
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
				
		print import_commands

	def main(self, args):
		parser = OptionParser(prog="N/A")
		parser.add_option("-f", "--format", help="Generate format")
		parser.add_option("-o", "--output", help="write report to FILE(s)")
		parser.add_option("--trac-path", help="The path to track (used for importing wikis)")
		(options, args) = parser.parse_args(args=args)
		if options.format in ["trac"]:
			self.generate_trac(options.output, options.trac_path)
		else:
			log("Invalid format: %s"%options.format)
			return

def __main__(args):
	global helper
	helper.main(args);
	
def init(plugin_id, plugin_alias, script_alias):
	global helper
	helper = DocumentationHelper(plugin_id, plugin_alias, script_alias)

def shutdown():
	None


