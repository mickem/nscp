from NSCP import Core
core = Core.get()

def __main__():
	# List all namespaces recursivly
	(ret, ns_msgs) = core.simple_exec('any', 'wmi', ['--list-all-ns', '--namespace', 'root'])
	for ns in ns_msgs[0].splitlines():
		# List all classes in each namespace
		(ret, cls_msgs) = core.simple_exec('any', 'wmi', ['--list-classes', '--simple', '--namespace', ns])
		for cls in cls_msgs[0].splitlines():
			print '%s : %s'%(ns, cls)
