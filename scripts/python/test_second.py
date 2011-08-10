from NSCP import Settings, Functions, log, status

def test(args):
	log('second: %s'%args)
	return (status.OK)

def init():
	log('Second says hello')
	fun.register_simple('second', test, 'This is a command from another script...')

def shutdown():
	log('Second says goodby...')
