import os
import sys
vcprojs = []
def scan_folder(f):
	for root, subFolders, files in os.walk(f):
		for file in files:
			if file.endswith('.vcproj'):
				vcprojs.append(os.path.join(root,file))
		for file in subFolders:
			scan_folder(os.path.join(root,file))

def replace_in_file(f, frm, to):
	with open(f, "r") as sources:
		lines = sources.readlines()
	with open(f, "w") as sources:
		for line in lines:
			sources.write(line.replace(frm, to))
	print "Replaced %s => %s in %s"%(frm, to, f)

scan_folder(os.getcwd())
for f in vcprojs:
	replace_in_file(f, 'RuntimeLibrary="3"', 'RuntimeLibrary="1"')
	replace_in_file(f, 'RuntimeLibrary="2"', 'RuntimeLibrary="0"')
