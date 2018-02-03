#!/usr/bin/python
import re
import sys
import os
from datetime import date
from optparse import OptionParser
from VersionHandler import VersionHandler

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
				  dest="update", 
				  help="Update a file (major, minor, revision, build)")

(options, args) = parser.parse_args()
version = None
if options.filename and options.create:
	version = VersionHandler(options.filename)
	version.create()
elif options.filename and options.update:
	version = VersionHandler(options.filename)
	version.read()
	version.increment(options.update)
	version.touch()
	version.write()
elif options.filename:
	version = VersionHandler(options.filename)
	version.read()
else:
	parser.print_help()

if version:
	if options.targetHPP:
		version.write_hpp(options.targetHPP)
	if options.display:
		version.print_version()

		
