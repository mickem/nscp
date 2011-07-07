import os
import sys

if len(sys.argv) == 1:
	print 'ERR: Need to specify RC version'
	sys.exit(1)
elif len(sys.argv) == 2:
	rc = sys.argv[1]

if rc == "--release":
	rc = ""

	
def rename_path(path):
	l = [(os.path.getmtime('%s/%s'%(path, x) ), x) for x in os.listdir(path)]
	l.sort()
	found = []
	for p in l:
		name = p[1]
		names = name.split('-')
		key = names[2]
		ext = names[-1].split('.')[-1]
		if not key in found:
			found.append(key)
			src ='%s/%s'%(path, name)
			if rc:
				tgt = '%s/%s-%s-%s-%s.%s'%(path, names[0], names[1], names[2], rc, ext)
			else:
				tgt = '%s/%s-%s-%s.%s'%(path, names[0], names[1], names[2], ext)
			if os.path.exists(tgt):
				print 'ERR: File already exists: %s'%tgt
			else:
				print '%s => %s'%(src, tgt)
				os.rename(src, tgt)


rename_path("stage/archive")
rename_path("stage/installer")
rename_path("stage/op5-installer")
rename_path("stage/opsera-installer")


