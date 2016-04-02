from NSCP import Settings, Registry, Core, log, status, log_error, sleep
from test_helper import BasicTest, TestResult, Callable, setup_singleton, install_testcases, init_testcases, shutdown_testcases
from types import *
import random
import subprocess
import uuid
import os
import sys, stat, datetime, time

class Win32FileTest(BasicTest):

	reg = None
	conf = None
	core = None

	def __init__(self):
		self.test_data = [
			['test.001', 		4, 	-5, ''],
			['test-001.txt', 	4, 	-5, ''],
			['test-002.txt', 	12,	-5, ''],
			['test-003.txt', 	32,	-10, ''],
			['test-004.txt', 	4, 	-10, ''],
			['test-005.txt', 	4, 	0, ''],
			['test-006.txt', 	4, 	5, ''],
			['test-007.txt', 	4, 	5, '001/002/003'],
			['test-008.txt', 	4, 	5, '001/002'],
			['test-009.txt', 	4, 	5, '001']
		]

	def desc(self):
		return 'Testcase for w32 check_file module'

	def title(self):
		return 'Win32File tests'

	def setup(self, plugin_id, prefix):
		self.reg = Registry.get(plugin_id)
		self.temp_path = self.core.expand_path('${temp}')
		log('Temp: %s'%self.temp_path)
		self.work_path = os.path.join(self.temp_path, '%s'%uuid.uuid4())
		log('Work: %s'%self.work_path)
		os.mkdir(self.work_path)

	def teardown(self):
		None
	
	def get_real_filename(self, name, path):
		if path != '':
			folder = os.path.join(self.work_path, path)
			return (folder, os.path.join(folder, name))
		else:
			return (self.work_path, os.path.join(self.work_path, name))
		
	def create_file(self, name, size, time_offset, path = ''):
		(folder, file_name) = self.get_real_filename(name, path)
		if not os.path.exists(folder):
			os.makedirs(folder)
		
		if not os.path.exists(file_name):
			f = open(file_name, 'w')
			for x in range(0,size):
				f.write('%d'%(x%10))
			f.close()
		
		today = datetime.datetime.now()
		pastday = today + datetime.timedelta(minutes=time_offset)
		atime = int(time.mktime(pastday.timetuple()))
		times = (atime,atime)
		os.utime(file_name,times)
		
	def delete_file(self, name, path = ''):
		(folder, file_name) = self.get_real_filename(name, path)
		if os.path.exists(file_name):
			try:
				os.remove(file_name)
			except OSError, (errno, strerror):
				log('Failed to delete: %s'%file_name)
		if os.path.exists(folder):
			try:
				os.rmdir(folder)
			except OSError, (errno, strerror):
				None

	def setup_files(self):
		for data in self.test_data:
			self.create_file(data[0], data[1], data[2], data[3])

	def cleanup_files(self):
		for data in self.test_data:
			self.delete_file(data[0], data[3])
		for data in self.test_data:
			self.delete_file(data[0], data[3])
		if os.path.exists(self.work_path):
			try:
				os.rmdir(self.work_path)
			except OSError, (errno, strerror):
				log('Failed to delete folder: %s'%self.work_path)
				log('Failed to delete folder: %s'%errno)
			
	def get_count(self,perf):
		if not perf:
			return -1
		(title, data) = perf.split('=')
		if not data:
			return -1
		(count, warn, crit) = data.split(';')
		return int(count)

	def check_files(self, filter, text, expected, extra_args):
		self.setup_files()
		alias = '%s: %s'%(text, filter)
		result = TestResult('Checking %s'%alias)
		args = ['path=%s'%self.work_path, 'filter=%s'%filter, 'syntax=%filename%: %size% %write%', 'warn=gt:1', 'crit=gt:3']
		args.extend(extra_args)
		(ret, msg, perf) = self.core.simple_query('CheckFiles', args)
		#log("Messge: %s"%msg)
		#log("Perf: %s"%perf)
		count = self.get_count(perf)
		result.add_message(count == expected, 'Check that we get correct number of files', 'Invalid result: got %s expected %s'%(count, expected))
		if expected > 3:
			result.add_message(ret == status.CRITICAL, 'Check that we get correct status back (CRIT)', 'We did not get a CRIT back as expected: %s'%ret)
		elif expected > 1:
			result.add_message(ret == status.WARNING, 'Check that we get correct status back (WARN)', 'We did not get a WARN back as expected: %s'%ret)
		elif expected > 0:
			result.add_message(ret == status.OK, 'Check that we get correct status back (OK)', 'We did not get a OK back as expected: %s'%ret)
		else:
			result.add_message(ret == status.UNKNOWN, 'Check that we get correct status back (UNKNOWN)', 'We did not get a UNKNOWN back as expected: %s'%ret)
		return result

	def check_no_files(self):
		self.setup_files()
		result = TestResult('Checking no files')
		args = ['path=%s\\aaa.txt'%self.work_path]
		(ret, msg, perf) = self.core.simple_query('check_files', args)
		#log("Messge: %s"%msg)
		#log("Perf: %s"%perf)
		result.add_message(ret == status.UNKNOWN, 'Check that we get correct status back', 'Return status was wrong: %s'%ret)
		#count = self.get_count(perf)
		result.assert_equals(msg, 'No files found', 'Validate return message')
			
		return result

	def run_test(self):
		result = TestResult('Testing W32 file systems')

		# Check size
		result.add(self.check_files('size gt 0b', 'Count all files (not folders)', 10, []))
		result.add(self.check_files('size gt 4b', 'Count all files > 4b', 2, []))
		result.add(self.check_files('size lt 5b', 'Count all files < 5b', 11, []))
		result.add(self.check_files('size eq 4b', 'Count all files = 4b', 8, []))
		result.add(self.check_files('size ne 4b', 'Count all files!= 4b', 5, []))
		result.add(self.check_files('size lt 4m', 'Count all files < 5m', 13, []))
		result.add(self.check_files('size eq 0b', 'Count all folders', 3, []))
		
		# Check flags (recursive, pattern)
		result.add(self.check_files('size eq 0b', 'Count all folders (non recursivly)', 3, ['max-dir-depth=0']))
		result.add(self.check_files('size eq 0b', 'Count all folders (recurse 1)', 1, ['max-dir-depth=1']))
		result.add(self.check_files('size eq 0b', 'Count all folders (recurse 1)', 2, ['max-dir-depth=2']))
		result.add(self.check_files('size eq 0b', 'Count all folders (recurse 1)', 3, ['max-dir-depth=3']))
		result.add(self.check_files('size eq 0b', 'Count all folders (recurse 1)', 3, ['max-dir-depth=4']))
		result.add(self.check_files('size gt 0b', 'Count all files (*.txt)', 9, ['pattern=*.txt']))
		result.add(self.check_files('size gt 0b', 'Count all files (*.foo)', 0, ['pattern=*.foo']))

		# Check dates
		result.add(self.check_files('written ge -5m', 'Count all files (*.txt, >-5m)', 7, ['pattern=*.txt']))
		result.add(self.check_files('written le -5m', 'Count all files (*.txt, <-5m)', 4, ['pattern=*.txt']))
		result.add(self.check_files('written lt -9m', 'Count all files (*.txt, <-9m)', 2, ['pattern=*.txt']))
		result.add(self.check_files('written gt -9m', 'Count all files (*.txt, >-9m)', 7, ['pattern=*.txt']))
		result.add(self.check_files('written lt -1m', 'Count all files (*.txt, <-1m)', 4, ['pattern=*.txt']))
		result.add(self.check_files('written gt -9m and written lt -1m', 'Count all files (*.txt, >-10m<-5m)', 2, ['pattern=*.txt']))
		result.add(self.check_files('written gt 0m', 'Count all files (*.txt, >0m)', 4, ['pattern=*.txt']))

		result.add(self.check_no_files())
		self.cleanup_files()
		
		return result

	def install(self, arguments):
		conf = self.conf
		conf.set_string('/modules', 'test_disk', 'CheckDisk')
		conf.set_string('/modules', 'pytest', 'PythonScript')
		conf.set_string('/settings/pytest/scripts', 'test_w32file', 'test_w32_file.py')
		conf.save()

	def uninstall(self):
		None

	def help(self):
		None

	def init(self, plugin_id, prefix):
		self.reg = Registry.get(plugin_id)
		self.core = Core.get(plugin_id)
		self.conf = Settings.get(plugin_id)

	def shutdown(self):
		None

setup_singleton(Win32FileTest)

all_tests = [Win32FileTest]

def __main__(args):
	install_testcases(all_tests)
	
def init(plugin_id, plugin_alias, script_alias):
	init_testcases(plugin_id, plugin_alias, script_alias, all_tests)

def shutdown():
	shutdown_testcases()
