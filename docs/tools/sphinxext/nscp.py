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
		module = self.env.temp_data.get('nscp:module')
		fullname = 'TODO'
		if self.objtype == 'query':
			fullname = '%s.%s'%(module, sig)
			signode['fullname'] = fullname
			signode += addnodes.desc_addname(module, module)
			signode += addnodes.desc_name(sig, sig)
			signode += addnodes.desc_content('')
			signode += addnodes.compact_paragraph(synopsis, synopsis)
		elif self.objtype == 'option':
			command = self.env.temp_data.get('nscp:command')
			fullname = '%s.%s:%s'%(module, command, sig)
			signode['fullname'] = fullname
			ann = ' (%s, %s)'%(module, command)
			signode += addnodes.desc_name(sig, sig)
			signode += addnodes.desc_annotation(ann, ann)
		elif self.objtype == 'confpath':
			fullname = '%s:%s'%(module, sig)
			signode['fullname'] = fullname
			ann = ' (%s)'%(module)
			signode += addnodes.desc_name(sig, sig)
			signode += addnodes.desc_annotation(ann, ann)
		elif self.objtype == 'confkey':
			confpath = self.env.temp_data.get('nscp:confpath', '')
			fullname = '%s:%s:%s'%(module, confpath, sig)
			signode['fullname'] = fullname
			ann = ' (%s, %s)'%(module, confpath)
			signode += addnodes.desc_name(sig, sig)
			signode += addnodes.desc_annotation(ann, ann)
		print 'handle_signature(%s, %s) => %s'%(sig, signode, fullname)
		return fullname, sig

	def _get_index_text(self, name):
		print "_get_index_text %s, %s" % (name, self.objtype)
		if self.objtype == 'query':
			return _('%s (%s)') % (name[1], self.env.temp_data['nscp:module'])
		elif self.objtype == 'exec':
			return _('%s (%s)') % (name[1], self.env.temp_data['nscp:module'])
		elif self.objtype == 'confkey':
			confpath = self.env.temp_data.get('nscp:confpath', '')
			print confpath
			return _('%s (%s, %s)') % (name[1], self.env.temp_data['nscp:module'], confpath)
		elif self.objtype == 'confpath':
			return _('%s (%s)') % (name[1], self.env.temp_data['nscp:module'])
		elif self.objtype == 'option':
			return _('%s (%s, %s)') % (name[1], self.env.temp_data['nscp:module'], self.env.temp_data['nscp:command'])
		else:
			return ''


	def add_target_and_index(self, name, sig, signode):
		print 'add_target_and_index(%s, %s, %s)'%(name, sig, signode)
		if self.objtype == 'query':
			self.env.temp_data['nscp:command'] = name[1]
			fullname = name[0]
			domain = 'queries'
		elif self.objtype == 'option':
			fullname = name[0]
			domain = 'options'
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
		print "DEBUG: add_target_and_index name = %s as %s" %(name, fullname)
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
		#pieces = string.split(name, '.')
		#if name not in self.state.document.ids:
		#	signode['names'].append(name)
		#	signode['ids'].append(name)
		#	signode['first'] = (not self.names)
		#	self.state.document.note_explicit_target(signode)
		#	if self.objtype =='query':
		#		qinv = self.env.domaindata['nscp']['queries']
		#		self.env.temp_data['nscp:command'] = name
		#		if name in qinv:
		#			self.env.warn(
		#				self.env.docname,
		#				'duplicate NSClient++ query object description of %s, ' % name +
		#				'other instance in ' + self.env.doc2path(qinv[name][0]),
		#				self.lineno)
		#		qinv[name] = (self.env.docname, self.objtype)
		#	elif self.objtype == 'option':
		#		finv = self.env.domaindata['nscp']['options']
		#		fname, arity = name.split('/')
		#		if '..' in arity:
		#			first, last = map(int, arity.split('..'))
		#		else:
		#			first = last = int(arity)
		#		for arity_index in range(first, last+1):
		#			if fname in finv and arity_index in finv[fname]:
		#				self.env.warn(
		#					self.env.docname,
		#					('duplicate Ada procedure description'
		#					'of %s, ') % name +
		#					'other instance in ' +
		#					self.env.doc2path(finv[fname][arity_index][0]),
		#					self.lineno)
		#			arities = finv.setdefault(fname, {})
		#			arities[arity_index] = (self.env.docname, name)
		#	else:
		#		oinv = self.env.domaindata['nscp']['objects']
		#		if name in oinv:
		#			self.env.warn(
		#				self.env.docname,
		#				'duplicate Ada object description of %s, ' % name +
		#				'other instance in ' + self.env.doc2path(oinv[name][0]),
		#				self.lineno)
		#		oinv[name] = (self.env.docname, self.objtype)

		#indextext = self._get_index_text(name)
		#if indextext:
		#	self.indexnode['entries'].append(('single', indextext, name, name))

		#plain_name = pieces[-1]
		#indextext = self._get_index_text(plain_name)
		#if indextext:
		#	self.indexnode['entries'].append(('single', indextext, name, plain_name))


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
		if not has_explicit_title:
			title = title.lstrip(':')   # only has a meaning for the target
			target = target.lstrip('~') # only has a meaning for the title
			# if the first character is a tilde, don't display the module/class
			# parts of the contents
			if title[0:1] == '~':
				title = title[1:]
				colon = title.rfind(':')
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
		'exec':   ObjType(l_('exec'),  'exe'),
		'module': ObjType(l_('module'),'m'),
		'confkey': ObjType(l_('confkey'),'confkey'),
	}

	# TODO: Keywords .. query:: ... ?
	directives = {
		'query':         AdaObject,
		'exec':          AdaObject,
		'option':        AdaObject,
		'module':        AdaModule,
		'confpath':      AdaObject,
		'confkey':       AdaObject,
		'currentmodule': AdaCurrentModule,
	}
	# TODO: ?
	roles = {
		'query':    AdaXRefRole(),
		'exe':      AdaXRefRole(),
		'module':   AdaXRefRole(),
		'option':   AdaXRefRole(),
		'confkey':  AdaXRefRole(),
		'confpath': AdaXRefRole(),
	}
	# Object stores
	initial_data = {
		'objects': {},     # (fullname, name) -> docname, objtype
		'options': {},     # fullname -> docname, objtype
		'queries' : {},    # fullname -> (docname, objtype)
		'procedures' : {}, # fullname -> arity -> (targetname, docname)
		'modules': {},     # modname -> docname, synopsis, platform, deprecated
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
		for fullname, funcs in self.data['queries'].items():
			if fn == docname:
				del self.data['queries'][fullname]
		for fullname, funcs in self.data['procedures'].items():
			for arity, (fn, _) in funcs.items():
				if fn == docname:
					del self.data['procedures'][fullname][arity]
			if not self.data['procedures'][fullname]:
				del self.data['procedures'][fullname]

	def _find_obj(self, env, modname, name, objtype, searchorder=0):
		"""
		Find a NSClient++ object for "name", perhaps using the given module and/or
		classname.
		"""
		print "_find_obj: modname: %s, name: %s, objtype: %s" % (modname, name, objtype)
		if not name:
			return None, None
		if objtype == 'module':
			if name in self.data['modules']:
				docname, synopsis, deprecated = self.data['modules'][name]
				return name, docname
			return None, None
			
		if objtype == 'query':
			if name in self.data['queries']:
				print "** Found direct query: %s"%name
				docname, objtype = self.data['queries'][name]
				return name, docname
			print 'Quriery not found: %s'%self.data['queries']
			return None, None
		if name in self.data['objects']:
			return name, self.data['objects'][name][0]
		print 'Found nothing for: %s'%name

		#if '/' in name:
		#	fname, arity = name.split('/')
		#	arity = int(arity)
		#else:
		#	fname = name
		#	arity = -1
		#if fname in self.data['queries']:
		#	return name, self.data['queries'][fname]
		#elif fname in self.data['procedures']:
		#	arities = self.data['procedures'][fname]
		#else:
		#	return None, None

		#if arity == -1:
		#	arity = min(arities)
		#if arity in arities:
		#	docname, targetname = arities[arity]
		#	return targetname, docname
		return None, None

	def resolve_xref(self, env, fromdocname, builder,
					typ, target, node, contnode):
		print 'resolve_xref(%s)'%node
		if typ == 'module' and target in self.data['modules']:
			docname, synopsis, deprecated = self.data['modules'].get(target, ('','',''))
			if docname:
				return make_refnode(builder, fromdocname, docname, target, contnode, synopsis)
			else:
				return None
		if typ == 'query' and target in self.data['modules']:
			docname, synopsis, deprecated = self.data['modules'].get(target, ('','',''))
			if not docname:
				return None
			else:
				title = '%s%s' % (synopsis, (deprecated and ' (deprecated)' or ''))
				return make_refnode(builder, fromdocname, docname,
									'module-' + target, contnode, title)
		else:
			modname = node.get('nscp:module')
			searchorder = node.hasattr('refspecific') and 1 or 0
			name, obj = self._find_obj(env, modname, target, typ, searchorder)
			if not obj:
				return None
			else:
				return make_refnode(builder, fromdocname, obj, name,
									contnode, name)

	def get_objects(self):
		print 'get_objects'
		for refname, (docname, type) in self.data['objects'].iteritems():
			print '%s, %s, %s'%(refname, docname, type)
			yield (refname, refname, type, docname, refname, 1)

def setup(app):
	app.add_domain(NSClientDomain)