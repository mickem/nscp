import os
import sys
vcprojs = []
slns = []
def scan_folder(f):
	print 'Looking in: %s'%f
	for root, subFolders, files in os.walk(f):
		for file in files:
			if file.endswith('.vcproj'):
				vcprojs.append(os.path.join(root,file))
			if file.endswith('.sln'):
				slns.append(os.path.join(root,file))
		for file in subFolders:
			scan_folder(os.path.join(root,file))

def replace_in_file(f, frm, to):
	with open(f, "r") as sources:
		lines = sources.readlines()
	with open(f, "w") as sources:
		for line in lines:
			sources.write(line.replace(frm, to))
	print "Replaced %s"%f

scan_folder(os.getcwd())
for f in vcprojs:
	replace_in_file(f, 'Version="9.00"', 'Version="8.00"')
	
for f in slns:
	replace_in_file(f, 'Microsoft Visual Studio Solution File, Format Version 10.00', 'Microsoft Visual Studio Solution File, Format Version 09.00')
