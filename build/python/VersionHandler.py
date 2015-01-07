#!/bin/python
import re
import sys
import os
from datetime import date


class VersionHandler:
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

		except IOError as e:
			print 'File not found: %s (%s)'%(self.file, e)
			sys.exit(1)

	def write(self):
		try:
			d = os.path.dirname(self.file)
			if not os.path.exists(d):
				os.makedirs(d)
			f = open(self.file, 'w')
			f.write('version=%d.%d.%d\n'%(self.major, self.minor, self.revision))
			f.write('build=%d\n'%(self.build))
			f.write('date=%s\n'%(self.date))
			f.close()
			
		except IOError as e:
			print 'Failed to update: %s (%s)'%(self.file, e)
			sys.exit(1)
		
		
	def readline(self, line):
		line = line.strip('\r\n\t ')
		if len(line) == 0:
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

	def datestr(self):
		return '%s'%self.date
		
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
		d = os.path.dirname(file)
		if not os.path.exists(d):
			os.makedirs(d)
		f = open(file, 'w')
		(ignored, filename) = os.path.split(file)
		name = filename.upper().replace('.', '_')

		f.write('#ifndef %s\n'%name)
		f.write('#define %s\n'%name)
		
		f.write('#define PRODUCTVER     %d,%d,%d,%d\n'%(self.major, self.minor, self.revision, self.build))
		f.write('#define STRPRODUCTVER  "%d.%d.%d.%d"\n'%(self.major, self.minor, self.revision, self.build))
		f.write('#define STRPRODUCTDATE "%s"\n'%(self.date))
		
		f.write('#endif // %s\n'%name)
		f.close()
