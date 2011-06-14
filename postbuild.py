import os
import zipfile
import fnmatch

BREAKPAD_FOUND = "TRUE"
BREAKPAD_DUMPSYMS_EXE = "D:/source/libraries/google-breakpad-svn/src/tools/windows/binaries/dump_syms.exe"
BUILD_TARGET_EXE_PATH = "D:/source/nscp/build/x64"

if BREAKPAD_FOUND == "TRUE":
	print "Gathering symbols..."
	matches = []
	for root, dirnames, filenames in os.walk("."):
		if 'stage' in root:
			for filename in fnmatch.filter(filenames, '*.pdb'):
				matches.append(os.path.join(root, filename))
	zip = zipfile.ZipFile('symbols.zip', "w")
	for f in matches:
		print "Processing: %s"%f
		out = f.replace('.pdb', '.sym')
		os.system("%s %s > %s"%(BREAKPAD_DUMPSYMS_EXE, f, out))
		zip.write(out)
	zip.close()
	
	
