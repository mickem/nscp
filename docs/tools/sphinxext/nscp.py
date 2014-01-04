# -*- coding: utf-8 -*-
"""
	NSCP domain.

	:copyright: Copyright 2013 by Michael Medin
"""

import re
import string
import unicodedata

from docutils import nodes
from docutils.parsers.rst import directives

from sphinx import addnodes
from sphinx.roles import XRefRole
from sphinx.locale import l_, _
from sphinx.directives import ObjectDescription
from sphinx.domains import Domain, ObjType, Index
from sphinx.util.compat import Directive
from sphinx.util.nodes import make_refnode
from sphinx.util.docfields import Field, TypedField

class AdaObject(ObjectDescription):
	"""
	Description of an NSClient++ object.
	"""

	option_spec = {
		'synopsis': lambda x: x,
		'module': lambda x: x,
	}
	doc_field_types = [
	#    TypedField('synopsis', label=l_('Synopsis'), names=('title', 'subject', 'desc', 'description'),),
	#    Field('returnvalue', label=l_('Returns'), has_arg=False,
	#          names=('returns', 'return')),
	#    Field('returntype', label=l_('Return type'), has_arg=False,
	#          names=('rtype',)),
	]

	def needs_arglist(self):
		return False

	def handle_signature(self, sig, signode):
		#synopsis = unicodedata.normalize('NFD', self.options.get('synopsis'))
		synopsis = self.options.get('synopsis')
		module = self.options.get('module')
		if not module:
			module = self.env.temp_data.get('nscp:module')
		else:
			self.env.temp_data['nscp:module'] = module
			
		confpath = self.env.temp_data.get('nscp:confpath')
		command = self.env.temp_data.get('nscp:command')
		fullname = 'TODO'
		if self.objtype == 'module':
			fullname = '%s'%(sig)
			signode['fullname'] = fullname
			#signode += addnodes.desc_addname(module, module)
			signode += addnodes.desc_name(sig, sig)
			signode += addnodes.desc_content('')
			signode += addnodes.compact_paragraph(synopsis, synopsis)
		elif self.objtype == 'query':
			fullname = '%s.%s'%(module, sig)
			signode['fullname'] = fullname
			signode += addnodes.desc_addname(module, module)
			signode += addnodes.desc_name(sig, sig)
			signode += addnodes.desc_content('')
			signode += addnodes.compact_paragraph(synopsis, synopsis)
		elif self.objtype == 'option':
			fullname = '%s.%s.%s'%(module, command, sig)
			signode['fullname'] = fullname
			ann = ' (%s, %s)'%(module, command)
			signode += addnodes.desc_name(sig, sig)
			signode += addnodes.desc_annotation(ann, ann)
		elif self.objtype == 'confpath':
			fullname = '%s.%s'%(module, sig)
			signode['fullname'] = fullname
			ann = ' (%s)'%(module)
			signode += addnodes.desc_name(sig, sig)
			signode += addnodes.desc_annotation(ann, ann)
		elif self.objtype == 'confkey':
			fullname = '%s.%s.%s'%(module, confpath, sig)
			signode['fullname'] = fullname
			ann = ' (%s, %s)'%(module, confpath)
			signode += addnodes.desc_name(sig, sig)
			signode += addnodes.desc_annotation(ann, ann)
		#print 'handle_signature(%s, %s) => %s'%(sig, signode, fullname)
		return fullname, sig

	def _get_index_text(self, name):
		#print "_get_index_text %s, %s" % (name, self.objtype)
		if self.objtype == 'module':
			return _('%s') % (name[1])
		elif self.objtype == 'query':
			return _('%s (%s)') % (name[1], self.env.temp_data['nscp:module'])
		elif self.objtype == 'exec':
			return _('%s (%s)') % (name[1], self.env.temp_data['nscp:module'])
		elif self.objtype == 'confkey':
			return _('%s (%s, %s)') % (name[1], self.env.temp_data['nscp:module'], self.env.temp_data['nscp:confpath'])
		elif self.objtype == 'confpath':
			return _('%s (%s)') % (name[1], self.env.temp_data['nscp:module'])
		elif self.objtype == 'option':
			return _('%s (%s, %s)') % (name[1], self.env.temp_data['nscp:module'], self.env.temp_data['nscp:command'])
		else:
			return ''


	def add_target_and_index(self, name, sig, signode):
		#print 'add_target_and_index(%s, %s, %s)'%(name, sig, signode)
		if self.objtype == 'module':
			self.env.temp_data['nscp:module'] = name[1]
			fullname = name[0]
			domain = 'objects'
		elif self.objtype == 'query':
			self.env.temp_data['nscp:command'] = name[1]
			fullname = name[0]
			domain = 'objects'
		elif self.objtype == 'option':
			fullname = name[0]
			domain = 'objects'
		elif self.objtype == 'confpath':
			self.env.temp_data['nscp:confpath'] = name[1]
			fullname = name[0]
			domain = 'objects'
		elif self.objtype == 'confkey':
			fullname = name[0]
			domain = 'objects'
		else:
			fullname = name[0]
			domain = 'objects'
		#print "DEBUG: add_target_and_index name = %s as %s" %(name, fullname)
		if fullname not in self.state.document.ids:
			signode['names'].append(fullname)
			signode['ids'].append(fullname)
			#signode['first'] = (not self.names)
			self.state.document.note_explicit_target(signode)
			qinv = self.env.domaindata['nscp'][domain]
			if name in qinv:
				self.env.warn(
					self.env.docname,
					'duplicate NSClient++ query object description of %s, ' % name[0] +
					'other instance in ' + self.env.doc2path(qinv[name][0]),
					self.lineno)
			qinv[fullname] = (self.env.docname, self.objtype)
		
		indextext = self._get_index_text(name)
		if indextext:
			self.indexnode['entries'].append(('single', indextext, fullname, ''))

class AdaModule(Directive):
	"""
	Directive to mark description of a new module.
	"""

	has_content = False
	required_arguments = 1
	optional_arguments = 0
	final_argument_whitespace = False
	option_spec = {
		'synopsis': lambda x: x,
		'module': lambda x: x,
		'noindex': directives.flag,
		'deprecated': directives.flag,
	}

	def run(self):
		env = self.state.document.settings.env
		modname = self.arguments[0].strip()
		noindex = 'noindex' in self.options
		env.temp_data['nscp:module'] = modname
		env.domaindata['nscp']['modules'][modname] = (env.docname, self.options.get('synopsis', ''), 'deprecated' in self.options)
		ret = []
		if not noindex:
			targetnode = nodes.target('', '', ids=['module-' + modname], ismod=True)
			self.state.document.note_explicit_target(targetnode)
			# the platform and synopsis aren't printed; in fact, they are only
			# used in the modindex currently
			ret.append(targetnode)
			indextext = _('%s (module)') % modname
			inode = addnodes.index(entries=[('single', indextext, modname, '')])
			ret.append(inode)
		return ret

class AdaCurrentModule(Directive):
	"""
	This directive is just to tell Sphinx that we're documenting
	stuff in module foo, but links to module foo won't lead here.
	"""

	has_content = False
	required_arguments = 1
	optional_arguments = 0
	final_argument_whitespace = False
	option_spec = {}

	def run(self):
		env = self.state.document.settings.env
		modname = self.arguments[0].strip()
		if modname == 'None':
			env.temp_data['nscp:module'] = None
		else:
			env.temp_data['nscp:module'] = modname
		return []

class AdaXRefRole(XRefRole):
	def process_link(self, env, refnode, has_explicit_title, title, target):
		refnode['nscp:module'] = env.temp_data.get('nscp:module')
		refnode['nscp:command'] = env.temp_data.get('nscp:command')
		refnode['nscp:confpath'] = env.temp_data.get('nscp:confpath')
		if not has_explicit_title:
			title = title.lstrip(':')   # only has a meaning for the target
			target = target.lstrip('~') # only has a meaning for the title
			# if the first character is a tilde, don't display the module/class
			# parts of the contents
			if title[0:1] == '~':
				title = title[1:]
				colon = title.rfind('.')
				if colon != -1:
					title = title[colon+1:]
		return title, target

class AdaModuleIndex(Index):
	"""
	Index subclass to provide the NSClient++ module index.
	"""

	name = 'modindex'
	localname = l_('NSClient++ Module Index')
	shortname = l_('NSClient++ modules')

	def generate(self, docnames=None):
		content = {}
		# list of all modules, sorted by module name
		modules = sorted(self.domain.data['modules'].iteritems(), key=lambda x: x[0].lower())
		# sort out collapsable modules
		prev_modname = ''
		num_toplevels = 0
		for modname, (docname, synopsis, deprecated) in modules:
			if docnames and docname not in docnames:
				continue

			entries = content.setdefault(modname[0].lower(), [])

			# name, grouptype, page, anchor, extra, qualifier, description
			qualifier = deprecated and _('Deprecated') or ''
			entries.append([modname, '', docname, modname, '', qualifier, synopsis])
			prev_modname = modname

		# sort by first letter
		content = sorted(content.iteritems())

		return content, False

class NSClientDomain(Domain):
	"""NSClient++ language domain."""
	name = 'nscp'
	label = 'NSClient++'

	object_types = {
		'query':  ObjType(l_('query'), 'query'),
		'module':  ObjType(l_('module'), 'module'),
		'exec':   ObjType(l_('exec'),  'exe'),
		'confkey': ObjType(l_('confkey'),'confkey'),
	}

	# TODO: Keywords .. query:: ... ?
	directives = {
		'query':         AdaObject,
		'module':         AdaObject,
		'exec':          AdaObject,
		'option':        AdaObject,
		'confpath':      AdaObject,
		'confkey':       AdaObject,
		'currentmodule': AdaCurrentModule,
	}
	# TODO: ?
	roles = {
		'query':    AdaXRefRole(),
		'module':   AdaXRefRole(),
		'exe':      AdaXRefRole(),
		'option':   AdaXRefRole(),
		'confkey':  AdaXRefRole(),
		'confpath': AdaXRefRole(),
	}
	# Object stores
	initial_data = {
		'objects': {},     # (fullname, name) -> docname, objtype

		'procedures' : {}, # fullname -> arity -> (targetname, docname)
		'modules': {},     # modname -> docname, synopsis, deprecated
	}
	indices = [
		AdaModuleIndex,
	]

	def clear_doc(self, docname):
		for fullname, (fn, _) in self.data['objects'].items():
			if fn == docname:
				del self.data['objects'][fullname]
		for modname, (fn, _, _) in self.data['modules'].items():
			if fn == docname:
				del self.data['modules'][modname]
		for fullname, funcs in self.data['procedures'].items():
			for arity, (fn, _) in funcs.items():
				if fn == docname:
					del self.data['procedures'][fullname][arity]
			if not self.data['procedures'][fullname]:
				del self.data['procedures'][fullname]

	def _find_obj(self, env, modname, commandname, confpath, name, objtype):
		"""
		Find a NSClient++ object for "name", perhaps using the given module and/or
		classname.
		"""
		#print "_find_obj: modname: %s, commandname: %s, name: %s, objtype: %s" % (modname, commandname, name, objtype)
		if not name:
			return None, None
		if objtype == 'module':
			if name in self.data['modules']:
				docname, synopsis, deprecated = self.data['modules'][name]
				return name, docname
			return None, None
			
		elif objtype == 'query':
			if name in self.data['objects']:
				docname, objtype = self.data['objects'][name]
				return name, docname
			tname = '%s.%s'%(modname, name)
			if tname in self.data['objects']:
				docname, objtype = self.data['objects'][tname]
				return tname, docname
			print "_find_obj: ERROR: QUERY: modname: %s, commandname: %s, name: %s, objtype: %s => %s" % (modname, commandname, name, objtype, self.data['objects'].keys())
			return None, None

		elif objtype == 'confpath':
			if name in self.data['objects']:
				return name, self.data['objects'][name]
			tname = '%s.%s'%(modname, name)
			if tname in self.data['objects']:
				docname, objtype = self.data['objects'][tname]
				return tname, docname
			#print "_find_obj: CONFPATH: modname: %s, commandname: %s, name: %s, objtype: %s => %s" % (modname, commandname, name, objtype, self.data['objects'].keys())
			return None, None

		elif objtype == 'confkey':
			if name in self.data['objects']:
				return name, self.data['objects'][name]
			if confpath:
				tname = '%s.%s.%s'%(modname, confpath, name)
			elif modname:
				tname = '%s.%s'%(modname, name)
			else:
				tname = name
			if tname in self.data['objects']:
				docname, objtype = self.data['objects'][tname]
				return tname, docname
			#print "_find_obj: CONFKEY: modname: %s, commandname: %s, confpath: %s, name: %s, objtype: %s => %s" % (modname, commandname, confpath, name, objtype, self.data['objects'].keys())
			return None, None

		elif objtype == 'option':
			if name in self.data['objects']:
				return name, self.data['objects'][name]
			tname = '%s.%s.%s'%(modname, commandname, name)
			if tname in self.data['objects']:
				docname, objtype = self.data['objects'][tname]
				return tname, docname
			print "_find_obj: ERROR: OPTION: modname: %s, commandname: %s, name: %s, objtype: %s => %s" % (modname, commandname, name, objtype, self.data['objects'].keys())
			return None, None

		#print "_find_obj: NOTHING: modname: %s, commandname: %s, name: %s, objtype: %s => NONE" % (modname, commandname, name, objtype)
		#if name in self.data['objects']:
		#	return name, self.data['objects'][name]
		#tname = '%s.%s'%(modname, name)
		#if tname in self.data['objects']:
		#	docname, objtype = self.data['objects'][tname]
		#	return tname, docname
		#print 'Found nothing for: %s'%name

		#if '/' in name:
		#	fname, arity = name.split('/')
		#	arity = int(arity)
		#else:
		#	fname = name
		#	arity = -1

		#if arity == -1:
		#	arity = min(arities)
		#if arity in arities:
		#	docname, targetname = arities[arity]
		#	return targetname, docname
		return None, None

	def resolve_xref(self, env, fromdocname, builder,
					typ, target, node, contnode):
		#print 'resolve_xref(%s)'%node
		if typ == 'module' and target in self.data['modules']:
			docname, synopsis, deprecated = self.data['modules'].get(target, ('','',''))
			if docname:
				return make_refnode(builder, fromdocname, docname, target, contnode, synopsis)
			else:
				return None
		#if typ == 'query' and target in self.data['modules']:
		#	docname, synopsis, deprecated = self.data['modules'].get(target, ('','',''))
		#	if not docname:
		#		return None
		#	else:
		#		title = '%s%s' % (synopsis, (deprecated and ' (deprecated)' or ''))
		#		return make_refnode(builder, fromdocname, docname,
		#							'module-' + target, contnode, title)
		else:
			modname = node.get('nscp:module')
			command = node.get('nscp:command')
			confpath = node.get('nscp:confpath')
			name, obj = self._find_obj(env, modname, command, confpath, target, typ)
			if not obj:
				return None
			else:
				return make_refnode(builder, fromdocname, obj, name,
									contnode, name)

	def get_objects(self):
		#print 'get_objects'
		for refname, (docname, type) in self.data['objects'].iteritems():
			#print '%s, %s, %s'%(refname, docname, type)
			yield (refname, refname, type, docname, refname, 1)

def setup(app):
	app.add_domain(NSClientDomain)