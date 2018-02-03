import os
import sys

file = sys.argv[1]

with open(file, "r") as source:
	lines = source.readlines()
with open(file, "w") as target:
	for line in lines:
		if line.startswith('CFLAG=') and not '-D_USING_V110_SDK71_' in line:
			line = line.replace('\n', '-D_USING_V110_SDK71_\n')
		elif line.startswith('LFLAGS=') and not '/subsystem:console,5.01' in line:
			line = line.replace('/subsystem:console', '/subsystem:console,5.01')
		target.write(line)
print "Patched openssl to work with xp in %s"%file
