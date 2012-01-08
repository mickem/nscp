import threading

class BadThread(threading.Thread):
	id = -1
	def __init__(self, id):
		self.id = id
		threading.Thread.__init__(self)

	def run(self):
		i = 0
		while(True):
			i = i + 1
			if i > 100000:
				print 'Processing: %d'%self.id
				i = 0

for x in xrange(1000):
	BadThread(x).start()