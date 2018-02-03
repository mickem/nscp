#!/usr/bin/env python
import os
import zipfile
from optparse import OptionParser

def zipdir(path, ziph):
	for root, dirs, files in os.walk(path):
		dir = os.path.relpath(root, path)
		for file in files:
			if file.endswith('.py'):
				ziph.write(os.path.join(root, file), os.path.join(dir, file))

if __name__ == '__main__':

	parser = OptionParser()
	parser.add_option("-s", "--source", help="source FOLDER to fiond source files to compress", metavar="SOURCE")
	parser.add_option("-t", "--target", help="target zip FILE to write", metavar="TARGET")
	(options, args) = parser.parse_args()
	
	if not options.target:
		options.target = os.getcwd()
	if os.path.isdir(options.target):
		options.target = os.path.join(options.target, 'python27.zip')

	if not options.source:
		print "Please specify source folder"
		exit(2)

	zipf = zipfile.ZipFile(options.target, 'w')
	zipdir(options.source, zipf)
	zipf.close()