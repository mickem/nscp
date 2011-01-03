#!/bin/python
import re
import sys
import os
from datetime import date
from optparse import OptionParser

class Version:
	def __init__(self, file):
		self.file = file
		self.major = 0
		self.minor = 0
		self.revision = 0
		self.build = 1
		self.touch()
		
	def read(self):
		try:
			f = open(self.file, 'r')
			lines = f.readlines()
			f.close()
			for line in lines:
				self.readline(line)
			
				#if line.find(' PRODUCTVER ') != -1:
				#	m = re.search('.*(\d),(\d),(\d),(\d).*', line)
				#	print 'Parsed: %s as %s'%(line, m.groups())
				#	return (int(m.group(1)), int(m.group(2)), int(m.group(3)), int(m.group(4)))
		except IOError as e:
			print 'File not found: %s (%s)'%(self.file, e)
			sys.exit(1)
		
	def readline(self, line):
		if len(line.strip('\n\t ')) == 0:
			return
		try:
			m = re.search('(.*)=(.*)$', line)
			if not m:
				print 'Failed to parse line: %s'%(line.strip('\n\t '))
				return
			self.set(m.group(1), m.group(2))
		except IndexError as e:
			print 'Failed to parse line: %s (%s)'%(line.strip('\n\t '),e)

	def set(self,k,v):
		if k == 'version':
			m = re.search('.*(\d).(\d).(\d).*', v)
			(self.major, self.minor, self.revision) = [int(e) for e in m.groups()]
		elif k == 'build':
			self.build = int(v)
		elif k == 'date':
			self.date = v
			
	def touch(self):
		today = date.today()
		self.date = today.isoformat()
		
	def version(self):
		return '%d.%d.%d.%d'%(self.major, self.minor, self.revision, self.build)
		
	def __str__(self):
		return 'version: %s, date %s'%(self.version(), self.date)

	def __repr__(self):
		return 'version: %s, date %s'%(self.version(), self.date)

	def increment(self, key):
		if key == 'build':
			self.build += 1
		elif key == 'revision':
			self.revision += 1
			self.build = 0
		elif key == 'minor':
			self.minor += 1
			self.revision = 0
			self.build = 0
		elif key == 'major':
			self.major += 1
			self.minor = 0
			self.revision = 0
			self.build = 0
		
	def print_version(self):
		print '%d.%d.%d.%d'%(self.major, self.minor, self.revision, self.build)
	
	def write_hpp(self, file):
		f = open(file, 'w')
		(ignored, filename) = os.path.split(file)
		name = filename.upper().replace('.', '_')

		f.write('#ifndef %s\n'%name)
		f.write('#define %s\n'%name)
		
		f.write('#define PRODUCTVER     %d,%d,%d,%d\n'%(self.major, self.minor, self.revision, self.build))
		f.write('#define STRPRODUCTVER  "%d,%d,%d,%d"\n'%(self.major, self.minor, self.revision, self.build))
		f.write('#define STRPRODUCTDATE "%s"\n'%(self.date))
		
		f.write('#endif // %s\n'%name)
		f.close()

def run():
	parser = OptionParser()
	parser.add_option("-f", "--file", dest="filename",
					  help="File to update version in", metavar="FILE")
	parser.add_option("-g", "--generate-hpp", dest="targetHPP",
					  help="Generate a HPP file")
	parser.add_option("-c", "--create", action="store_true", dest="create",
					  help="Create a new file")
	parser.add_option("-d", "--display", action="store_true", dest="display",
					  help="Display the current version")
	parser.add_option("-u", "--update",
					  dest="update", default='build',
					  help="Update a file (major, minor, revision, build)")

	(options, args) = parser.parse_args()
	version = None
	if options.filename and options.create:
		version = Version(options.filename)
		version.create()
	elif options.filename and options.update:
		version = Version(options.filename)
		version.read()
		version.increment(options.update)
	else:
		parser.print_help()

	if options.targetHPP:
		version.write_hpp(options.targetHPP)
	if options.display:
		version.print_version()

		
run()