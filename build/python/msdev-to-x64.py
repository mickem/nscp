import os
import sys
vcprojs = []
slns = []
def scan_folder(f):
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
	print "Replaced %s => %s in %s"%(frm, to, f)

scan_folder(os.getcwd())
for f in vcprojs:
	replace_in_file(f, 'Name="Win32"', 'Name="x64"')
	replace_in_file(f, 'Name="Debug|Win32"', 'Name="Debug|x64"')
	replace_in_file(f, 'Name="Release|Win32"', 'Name="Release|x64"')
	replace_in_file(f, 'DebugInformationFormat="4"', 'DebugInformationFormat="3"')
	replace_in_file(f, 'TargetMachine="1"', 'TargetMachine="17"')
	#replace_in_file(f, 'Name="VCMIDLTool"', 'Name="VCMIDLTool" TargetEnvironment="3"')
	
	
for f in slns:
	replace_in_file(f, 'Win32', 'x64')
