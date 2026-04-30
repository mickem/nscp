from NSCP import Settings, Registry, Core, log, status, log_error, sleep
from test_helper import BasicTest, TestResult, install_testcases, init_testcases, shutdown_testcases
import uuid
import os
import shutil
import datetime, time

class Win32FileTest(BasicTest):

	reg = None
	conf = None
	core = None

	def __init__(self):
		BasicTest.__init__(self)
		self.temp_path = None
		self.work_path = None
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
		pass

	def get_real_filename(self, name : str, path : str):
		if path != '':
			folder = os.path.join(self.work_path, path)
			return folder, os.path.join(folder, name)
		else:
			return self.work_path, os.path.join(self.work_path, name)
		
	def create_file(self, name, size, time_offset, path = ''):
		(folder, file_name) = self.get_real_filename(name, path)
		if not os.path.exists(folder):
			os.makedirs(folder)
		
		if not os.path.exists(file_name):
			print(f"Creating {file_name} of size {size}")
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
			except OSError:
				log('Failed to delete: %s'%file_name)
		if os.path.exists(folder):
			try:
				os.rmdir(folder)
			except OSError:
				pass

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
			except OSError as e:
				log('Failed to delete folder: %s'%self.work_path)
				log('Failed to delete folder: %s'%e)
			
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
		args = ['path=%s'%self.work_path, 'filter=%s'%filter,
			'detail-syntax=%(filename): %(size) %(written)',
			'warn=count > 1', 'crit=count > 3',
			'empty-state=unknown']
		args.extend(extra_args)
		(ret, msg, perf) = self.core.simple_query('check_files', args)
		log("Messge: %s"%msg)
		log("Perf: %s"%perf)
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
		# Pointing `path=` at something that does not exist must surface as
		# UNKNOWN with the modern "Path was not found: <path>" diagnostic
		# (issue #613). The legacy "No files found" semantics no longer
		# apply here because the path itself is missing rather than empty.
		args = ['path=%s\\aaa.txt'%self.work_path]
		(ret, msg, perf) = self.core.simple_query('check_files', args)
		result.add_message(ret == status.UNKNOWN, 'Check that we get correct status back', 'Return status was wrong: %s'%ret)
		result.assert_contains('Path was not found', msg, 'Validate return message')

		return result

	# ------------------------------------------------------------------
	# Helpers shared by the bug-fix regression tests below.
	# ------------------------------------------------------------------

	def _scratch(self, name):
		"""Return a unique sub-folder under work_path; create it fresh.

		Keeping each regression test in its own scratch directory avoids
		interaction with the legacy `setup_files()` fixture set and lets us
		clean up surgically without affecting the other tests.
		"""
		path = os.path.join(self.work_path, name)
		if os.path.exists(path):
			shutil.rmtree(path, ignore_errors=True)
		os.makedirs(path)
		return path

	def _remove_scratch(self, path):
		if path and os.path.exists(path):
			shutil.rmtree(path, ignore_errors=True)

	def _write(self, path, contents = 'hello'):
		with open(path, 'w', encoding='utf-8') as f:
			f.write(contents)

	def _query(self, command, args):
		log('Running: %s %s' % (command, ' '.join(args)))
		(ret, msg, perf) = self.core.simple_query(command, args)
		log('  -> ret=%s msg=%r perf=%r' % (ret, msg, perf))
		return (ret, msg, perf)

	def check_max_depth_zero(self):
		"""Issue #730: max-depth=0 must scan the top directory only,
		not return "no files found"."""
		result = TestResult('Issue #730: max-depth=0 scans top directory only')
		root = self._scratch('depth0')
		try:
			self._write(os.path.join(root, 'top1.txt'))
			self._write(os.path.join(root, 'top2.txt'))
			os.makedirs(os.path.join(root, 'sub'))
			self._write(os.path.join(root, 'sub', 'nested.txt'))

			args = ['path=%s' % root, 'max-depth=0', "filter=type='file'",
					'warn=count > 5', 'crit=count > 10', 'top-syntax=%(count) files']
			(ret, msg, perf) = self._query('check_files', args)
			result.add_message(ret == status.OK,
				'max-depth=0 returns OK',
				'Expected OK, got %s (%s)' % (ret, msg))
			result.assert_contains('2 files', msg,
				'max-depth=0 must count exactly the 2 top-level files')
			result.assert_not_contains('nested.txt', msg,
				'max-depth=0 must not descend into subdirectories')
		finally:
			self._remove_scratch(root)
		return result

	def check_missing_path_unknown(self):
		"""Issue #613: a non-existent top-level path must be reported as
		UNKNOWN, not silently produce OK / "No files found"."""
		result = TestResult('Issue #613: missing path -> UNKNOWN')
		# Path is intentionally NOT created.
		missing = os.path.join(self.work_path, 'does_not_exist_47b1f0e5')
		args = ['path=%s' % missing, "filter=type='file'",
				'warn=count > 5', 'crit=count > 10']
		(ret, msg, perf) = self._query('check_files', args)
		result.add_message(ret == status.UNKNOWN,
			'Missing path returns UNKNOWN',
			'Expected UNKNOWN, got %s (%s)' % (ret, msg))
		result.assert_contains('Path was not found', msg,
			'Missing path must be named in the response')
		return result

	def check_missing_path_in_list_unknown(self):
		"""Issue #613 variant: a missing path in a list of otherwise-valid
		paths must still surface as UNKNOWN."""
		result = TestResult('Issue #613: missing path among many -> UNKNOWN')
		good = self._scratch('mixed_paths')
		try:
			self._write(os.path.join(good, 'a.txt'))
			missing = os.path.join(self.work_path, 'does_not_exist_47b1f0e5')
			args = ['path=%s' % good, 'path=%s' % missing,
					"filter=type='file'", 'warn=count > 0', 'crit=count > 100000']
			(ret, msg, perf) = self._query('check_files', args)
			result.add_message(ret == status.UNKNOWN,
				'Mixed valid+missing paths return UNKNOWN',
				'Expected UNKNOWN, got %s (%s)' % (ret, msg))
			result.assert_contains('Path was not found', msg,
				'Missing path must be named in the response')
		finally:
			self._remove_scratch(good)
		return result

	def check_files_legacy_empty_state_ok(self):
		"""Issue #717: the legacy `CheckFiles` shim must default empty
		results to OK (not UNKNOWN) when only MaxWarn/MaxCrit are set."""
		result = TestResult('Issue #717: legacy CheckFiles 0-match -> OK')
		empty = self._scratch('empty_legacy')
		try:
			args = ['path=%s' % empty, 'pattern=*.log',
					'MaxWarn=10', 'MaxCrit=20']
			(ret, msg, perf) = self._query('CheckFiles', args)
			result.add_message(ret == status.OK,
				'Legacy CheckFiles with 0 matches returns OK',
				'Expected OK, got %s (%s)' % (ret, msg))
		finally:
			self._remove_scratch(empty)
		return result

	def check_non_acp_path_files(self):
		"""Issue #598: paths containing characters outside the active code
		page must be readable. The path uses CJK characters, which are
		outside CP1252/CP1250 and so trigger the original bug.
		"""
		result = TestResult('Issue #598: non-ACP path is readable (check_files)')
		root = None
		try:
			root = self._scratch(u'\u65e5\u672c\u8a9e_files')  # 日本語_files
			self._write(os.path.join(root, 'file.txt'), 'data')

			args = ['path=%s' % root, "filter=type='file'",
					'warn=count > 5', 'crit=count > 10',
					'top-syntax=%(count) files']
			(ret, msg, perf) = self._query('check_files', args)
			result.add_message(ret == status.OK,
				'Non-ACP path is read successfully',
				'Expected OK, got %s (%s)' % (ret, msg))
			result.assert_contains('1 files', msg,
				'Non-ACP path must produce the expected count')
		except OSError as e:
			# Some filesystems / build agents cannot create the path; treat
			# that as skipped rather than failed so the suite stays green.
			result.add_message(True,
				'Skipped: filesystem could not create non-ACP directory (%s)' % e)
		finally:
			self._remove_scratch(root)
		return result

	# ------------------------------------------------------------------
	# Additional check_files edge cases.
	# ------------------------------------------------------------------

	def check_files_modern_empty_state_ok(self):
		"""Modern check_files must honour empty-state=ok on an empty directory.

		Mirrors the legacy #717 test from the modern entry point: when the
		caller explicitly opts in to empty-state=ok the result is OK rather
		than the default UNKNOWN.
		"""
		result = TestResult('check_files: empty-state=ok overrides empty -> OK')
		empty = self._scratch('empty_modern')
		try:
			args = ['path=%s' % empty, 'pattern=*.log', 'empty-state=ok']
			(ret, msg, perf) = self._query('check_files', args)
			result.add_message(ret == status.OK,
				'empty-state=ok on empty dir returns OK',
				'Expected OK, got %s (%s)' % (ret, msg))
		finally:
			self._remove_scratch(empty)
		return result

	def check_files_paths_comma_separated(self):
		"""`paths=A,B` (single argument, comma-separated) must scan both
		directories. This is a separate code path from repeated `path=`.
		"""
		result = TestResult('check_files: paths=A,B comma-separated form')
		a = self._scratch('paths_a')
		b = self._scratch('paths_b')
		try:
			self._write(os.path.join(a, 'one.txt'))
			self._write(os.path.join(b, 'two.txt'))
			self._write(os.path.join(b, 'three.txt'))
			args = ['paths=%s,%s' % (a, b), "filter=type='file'",
					'warn=count > 100', 'crit=count > 200',
					'top-syntax=%(count) files']
			(ret, msg, perf) = self._query('check_files', args)
			result.add_message(ret == status.OK,
				'paths=A,B returns OK',
				'Expected OK, got %s (%s)' % (ret, msg))
			result.assert_contains('3 files', msg,
				'paths=A,B must aggregate files from both paths (1+2=3)')
		finally:
			self._remove_scratch(a)
			self._remove_scratch(b)
		return result

	def check_files_file_alias(self):
		"""`file=` is documented as an alias for `path=`."""
		result = TestResult('check_files: file= is an alias for path=')
		root = self._scratch('file_alias')
		try:
			self._write(os.path.join(root, 'a.txt'))
			args = ['file=%s' % root, "filter=type='file'",
					'warn=count > 100', 'crit=count > 200',
					'top-syntax=%(count) files']
			(ret, msg, perf) = self._query('check_files', args)
			result.add_message(ret == status.OK,
				'file= alias returns OK',
				'Expected OK, got %s (%s)' % (ret, msg))
			result.assert_contains('1 files', msg,
				'file= alias must enumerate the same files as path=')
		finally:
			self._remove_scratch(root)
		return result

	def check_files_path_is_a_file(self):
		"""Pinning behaviour: passing a regular file as `path=` is accepted
		and counted as a single match (recursive_scan handles a leaf file
		as a one-element directory)."""
		result = TestResult('check_files: path=<file> -> OK with count 1')
		root = self._scratch('path_is_file')
		try:
			f = os.path.join(root, 'lonely.txt')
			self._write(f, 'hello')
			args = ['path=%s' % f, "filter=type='file'",
					'warn=count > 5', 'crit=count > 10',
					'top-syntax=%(count) files']
			(ret, msg, perf) = self._query('check_files', args)
			result.add_message(ret == status.OK,
				'A file path is accepted and reported as OK',
				'Expected OK, got %s (%s)' % (ret, msg))
			result.assert_contains('1 files', msg,
				'A file path must be counted as one matching file')
		finally:
			self._remove_scratch(root)
		return result

	def check_files_invalid_filter_syntax(self):
		"""A clearly malformed filter expression must surface as UNKNOWN
		with `Failed to parse syntax` (cli_helper::build_filter)."""
		result = TestResult('check_files: invalid filter -> UNKNOWN')
		root = self._scratch('bad_filter')
		try:
			self._write(os.path.join(root, 'a.txt'))
			args = ['path=%s' % root, 'filter=size >>> 5b',
					'warn=count > 100', 'crit=count > 200']
			(ret, msg, perf) = self._query('check_files', args)
			result.add_message(ret == status.UNKNOWN,
				'Invalid filter expression returns UNKNOWN',
				'Expected UNKNOWN, got %s (%s)' % (ret, msg))
		finally:
			self._remove_scratch(root)
		return result

	def check_files_trailing_backslash(self):
		"""Trailing backslash on a path must be tolerated (common in NRPE
		configs that build the path by string concatenation)."""
		result = TestResult('check_files: trailing backslash is tolerated')
		root = self._scratch('trailing_bs')
		try:
			self._write(os.path.join(root, 'a.txt'))
			self._write(os.path.join(root, 'b.txt'))
			path_with_bs = root + '\\'
			args = ['path=%s' % path_with_bs, "filter=type='file'",
					'warn=count > 100', 'crit=count > 200',
					'top-syntax=%(count) files']
			(ret, msg, perf) = self._query('check_files', args)
			result.add_message(ret == status.OK,
				'Path with trailing backslash returns OK',
				'Expected OK, got %s (%s)' % (ret, msg))
			result.assert_contains('2 files', msg,
				'Trailing backslash must not change the file count')
		finally:
			self._remove_scratch(root)
		return result

	def check_files_forward_slashes(self):
		"""Forward slashes in a Windows path must be tolerated (common in
		NRPE configs originating from Linux clients)."""
		result = TestResult('check_files: forward slashes are tolerated')
		root = self._scratch('forward_slashes')
		try:
			self._write(os.path.join(root, 'a.txt'))
			self._write(os.path.join(root, 'b.txt'))
			fwd = root.replace('\\', '/')
			args = ['path=%s' % fwd, "filter=type='file'",
					'warn=count > 100', 'crit=count > 200',
					'top-syntax=%(count) files']
			(ret, msg, perf) = self._query('check_files', args)
			result.add_message(ret == status.OK,
				'Path with forward slashes returns OK',
				'Expected OK, got %s (%s)' % (ret, msg))
			result.assert_contains('2 files', msg,
				'Forward slashes must not change the file count')
		finally:
			self._remove_scratch(root)
		return result

	def check_files_show_all_flag(self):
		"""`show-all` must keep the OK status while still producing output
		(it expands the rendered list, but does not promote OKs to a
		problem state)."""
		result = TestResult('check_files: show-all keeps OK status')
		root = self._scratch('show_all')
		try:
			for n in ('a.txt', 'b.txt', 'c.txt'):
				self._write(os.path.join(root, n))
			args = ['path=%s' % root, "filter=type='file'",
					'warn=count > 100', 'crit=count > 200',
					'show-all']
			(ret, msg, perf) = self._query('check_files', args)
			result.add_message(ret == status.OK,
				'show-all on an all-OK scan keeps status OK',
				'Expected OK, got %s (%s)' % (ret, msg))
		finally:
			self._remove_scratch(root)
		return result

	def check_files_glob_in_path(self):
		"""Pinning behaviour: a glob in `path=` is treated as a literal
		filesystem path (which does not exist) and surfaces as UNKNOWN.
		Globbing belongs in `pattern=`, not `path=`.
		"""
		result = TestResult('check_files: glob in path is a literal -> UNKNOWN')
		root = self._scratch('glob_in_path')
		try:
			self._write(os.path.join(root, 'a.log'))
			self._write(os.path.join(root, 'b.log'))
			args = ['path=%s\\*.log' % root, "filter=type='file'",
					'warn=count > 100', 'crit=count > 200']
			(ret, msg, perf) = self._query('check_files', args)
			result.add_message(ret == status.UNKNOWN,
				'Glob in path= is rejected as missing path',
				'Expected UNKNOWN, got %s (%s)' % (ret, msg))
		finally:
			self._remove_scratch(root)
		return result

	# ------------------------------------------------------------------
	# Acceptance tests for the new `check_single_file` command.
	# ------------------------------------------------------------------

	def check_single_file_ok(self):
		"""Small file under thresholds must return OK."""
		result = TestResult('check_single_file: file under threshold -> OK')
		root = self._scratch('single_ok')
		try:
			small = os.path.join(root, 'small.txt')
			self._write(small, 'a few bytes of text')
			args = ['file=%s' % small, 'warn=size > 1M', 'crit=size > 10M']
			(ret, msg, perf) = self._query('check_single_file', args)
			result.add_message(ret == status.OK,
				'check_single_file under threshold returns OK',
				'Expected OK, got %s (%s)' % (ret, msg))
			result.assert_contains('small.txt', msg,
				'Filename must appear in the rendered detail line')
		finally:
			self._remove_scratch(root)
		return result

	def check_single_file_warning(self):
		"""File over the warning threshold must return WARNING."""
		result = TestResult('check_single_file: file over warn threshold -> WARNING')
		root = self._scratch('single_warn')
		try:
			big = os.path.join(root, 'big.txt')
			# 200 KiB of payload reliably exceeds a 100k warn threshold.
			self._write(big, 'x' * 200000)
			args = ['file=%s' % big, 'warn=size > 100k', 'crit=size > 10M']
			(ret, msg, perf) = self._query('check_single_file', args)
			result.add_message(ret == status.WARNING,
				'check_single_file over warn threshold returns WARNING',
				'Expected WARNING, got %s (%s)' % (ret, msg))
			result.assert_contains('big.txt', msg,
				'Filename must appear in the rendered detail line')
		finally:
			self._remove_scratch(root)
		return result

	def check_single_file_missing(self):
		"""Missing file must return UNKNOWN with a useful message."""
		result = TestResult('check_single_file: missing file -> UNKNOWN')
		missing = os.path.join(self.work_path, 'single_missing', 'nope.txt')
		args = ['file=%s' % missing, 'warn=size > 1M']
		(ret, msg, perf) = self._query('check_single_file', args)
		result.add_message(ret == status.UNKNOWN,
			'check_single_file on missing file returns UNKNOWN',
			'Expected UNKNOWN, got %s (%s)' % (ret, msg))
		result.assert_contains('File not found', msg,
			'Response must say "File not found"')
		return result

	def check_single_file_no_arg(self):
		"""No `file=` argument must return UNKNOWN."""
		result = TestResult('check_single_file: no file argument -> UNKNOWN')
		args = ['warn=size > 1M']
		(ret, msg, perf) = self._query('check_single_file', args)
		result.add_message(ret == status.UNKNOWN,
			'check_single_file without file= returns UNKNOWN',
			'Expected UNKNOWN, got %s (%s)' % (ret, msg))
		result.assert_contains('No file specified', msg,
			'Response must say "No file specified"')
		return result

	def check_single_file_non_acp_path(self):
		"""Issue #598 in `check_single_file`: a non-ACP filename must be
		readable."""
		result = TestResult('Issue #598: non-ACP path is readable (check_single_file)')
		root = None
		try:
			root = self._scratch(u'\u65e5\u672c\u8a9e_single')  # 日本語_single
			f = os.path.join(root, u'\u65e5\u672c\u8a9e.txt')   # 日本語.txt
			self._write(f, 'data')
			args = ['file=%s' % f, 'warn=size > 1M']
			(ret, msg, perf) = self._query('check_single_file', args)
			result.add_message(ret == status.OK,
				'Non-ACP file path returns OK',
				'Expected OK, got %s (%s)' % (ret, msg))
			result.assert_not_contains('File not found', msg,
				'Non-ACP path must not be reported as missing')
		except OSError as e:
			result.add_message(True,
				'Skipped: filesystem could not create non-ACP file (%s)' % e)
		finally:
			self._remove_scratch(root)
		return result

	def check_single_file_critical(self):
		"""crit threshold escalates to CRITICAL above warn."""
		result = TestResult('check_single_file: crit threshold -> CRITICAL')
		root = self._scratch('single_crit')
		try:
			f = os.path.join(root, 'huge.dat')
			self._write(f, 'x' * 200000)  # ~200 KiB
			# 100k warn, 100k crit: file size triggers both, so crit wins.
			args = ['file=%s' % f, 'warn=size > 50k', 'crit=size > 100k']
			(ret, msg, perf) = self._query('check_single_file', args)
			result.add_message(ret == status.CRITICAL,
				'crit threshold returns CRITICAL',
				'Expected CRITICAL, got %s (%s)' % (ret, msg))
		finally:
			self._remove_scratch(root)
		return result

	def check_single_file_age_threshold(self):
		"""Issue #598 also touched the date conversion path; pin it via an
		age-based threshold against a file whose mtime is 2 days in the
		past.
		"""
		result = TestResult('check_single_file: age threshold -> CRITICAL')
		root = self._scratch('single_age')
		try:
			f = os.path.join(root, 'old.txt')
			self._write(f, 'old')
			two_days_ago = time.time() - 2 * 24 * 3600
			os.utime(f, (two_days_ago, two_days_ago))
			args = ['file=%s' % f, 'crit=written < -1d']
			(ret, msg, perf) = self._query('check_single_file', args)
			result.add_message(ret == status.CRITICAL,
				'2-day-old file with crit=written < -1d returns CRITICAL',
				'Expected CRITICAL, got %s (%s)' % (ret, msg))
		finally:
			self._remove_scratch(root)
		return result

	def check_single_file_empty_file(self):
		"""A 0-byte file must still be discovered, not reported as missing."""
		result = TestResult('check_single_file: 0-byte file is OK')
		root = self._scratch('single_empty')
		try:
			f = os.path.join(root, 'empty.dat')
			# Create a genuinely empty file.
			open(f, 'w').close()
			args = ['file=%s' % f, 'warn=size > 1M', 'crit=size > 10M']
			(ret, msg, perf) = self._query('check_single_file', args)
			result.add_message(ret == status.OK,
				'0-byte file returns OK',
				'Expected OK, got %s (%s)' % (ret, msg))
			result.assert_not_contains('File not found', msg,
				'A 0-byte file must not be reported as missing')
		finally:
			self._remove_scratch(root)
		return result

	def check_single_file_against_directory(self):
		"""Pinning behaviour: passing a directory to check_single_file
		must produce a deterministic result rather than crash. Today
		stat_single_file rejects directories and we get UNKNOWN."""
		result = TestResult('check_single_file: directory argument -> UNKNOWN')
		root = self._scratch('single_dir')
		try:
			args = ['file=%s' % root, 'warn=size > 1M']
			(ret, msg, perf) = self._query('check_single_file', args)
			result.add_message(ret == status.UNKNOWN,
				'Directory passed to check_single_file returns UNKNOWN',
				'Expected UNKNOWN, got %s (%s)' % (ret, msg))
		finally:
			self._remove_scratch(root)
		return result

	def check_single_file_perfdata_shape(self):
		"""Perfdata for check_single_file must include the file size as a
		byte-scaled metric. The metric key is auto-prefixed with the
		filename (e.g. `'mid.dat size'=0.001MB;1;10`), so the assertion
		matches the bare `size` token rather than a literal `size=`.
		"""
		result = TestResult('check_single_file: perfdata exposes size metric')
		root = self._scratch('single_perf')
		try:
			f = os.path.join(root, 'mid.dat')
			self._write(f, 'x' * 1024)
			args = ['file=%s' % f, 'warn=size > 1M', 'crit=size > 10M']
			(ret, msg, perf) = self._query('check_single_file', args)
			result.add_message(ret == status.OK,
				'Perfdata sample query returns OK',
				'Expected OK, got %s (%s)' % (ret, msg))
			result.assert_contains('size', perf or '',
				'Perfdata must include a size metric')
		finally:
			self._remove_scratch(root)
		return result

	def run_test(self, cases = None):
		result = TestResult('Testing W32 file systems')

		# Check size
		sub_result = TestResult('Check Size')
		sub_result.add(self.check_files('size gt 0b', 'Count all files (not folders)', 10, []))
		sub_result.add(self.check_files('size gt 4b', 'Count all files > 4b', 2, []))
		sub_result.add(self.check_files('size lt 5b', 'Count all files < 5b', 11, []))
		sub_result.add(self.check_files('size eq 4b', 'Count all files = 4b', 8, []))
		sub_result.add(self.check_files('size ne 4b', 'Count all files!= 4b', 5, []))
		sub_result.add(self.check_files('size lt 4m', 'Count all files < 5m', 13, []))
		sub_result.add(self.check_files('size eq 0b', 'Count all folders', 3, []))
		result.append(sub_result)

	# Check flags (recursive, pattern)
		# Modern check_files uses `max-depth=` (the legacy CheckFiles flag
		# was `max-dir-depth=`). Note: legacy `max-dir-depth=0` meant
		# "unlimited" (the translator dropped the arg when the value was 0),
		# whereas modern `max-depth=0` means "top directory only". The
		# "non recursivly" entry below preserves the original intent
		# ("count every folder, no depth cap") by simply omitting the flag.
		sub_result = TestResult('Check flags (recursive, pattern)')
		sub_result.add(self.check_files('size eq 0b', 'Count all folders (non recursively)', 3, []))
		sub_result.add(self.check_files('size eq 0b', 'Count all folders (recurse 1)', 1, ['max-depth=1']))
		sub_result.add(self.check_files('size eq 0b', 'Count all folders (recurse 1)', 2, ['max-depth=2']))
		sub_result.add(self.check_files('size eq 0b', 'Count all folders (recurse 1)', 3, ['max-depth=3']))
		sub_result.add(self.check_files('size eq 0b', 'Count all folders (recurse 1)', 3, ['max-depth=4']))
		sub_result.add(self.check_files('size gt 0b', 'Count all files (*.txt)', 9, ['pattern=*.txt']))
		sub_result.add(self.check_files('size gt 0b', 'Count all files (*.foo)', 0, ['pattern=*.foo']))
		result.append(sub_result)

		# Check dates
		sub_result = TestResult('Check dates')
		sub_result.add(self.check_files('written ge -5m', 'Count all files (*.txt, >-5m)', 7, ['pattern=*.txt']))
		sub_result.add(self.check_files('written le -5m', 'Count all files (*.txt, <-5m)', 4, ['pattern=*.txt']))
		sub_result.add(self.check_files('written lt -9m', 'Count all files (*.txt, <-9m)', 2, ['pattern=*.txt']))
		sub_result.add(self.check_files('written gt -9m', 'Count all files (*.txt, >-9m)', 7, ['pattern=*.txt']))
		sub_result.add(self.check_files('written lt -1m', 'Count all files (*.txt, <-1m)', 4, ['pattern=*.txt']))
		sub_result.add(self.check_files('written gt -9m and written lt -1m', 'Count all files (*.txt, >-10m<-5m)', 2, ['pattern=*.txt']))
		sub_result.add(self.check_files('written gt 0m', 'Count all files (*.txt, >0m)', 4, ['pattern=*.txt']))
		result.append(sub_result)

		sub_result = TestResult('Legacy check')
		sub_result.add(self.check_no_files())
		self.cleanup_files()
		result.append(sub_result)

		sub_result = TestResult('Check Various flags')
		sub_result.add(self.check_max_depth_zero())
		sub_result.add(self.check_missing_path_unknown())
		sub_result.add(self.check_missing_path_in_list_unknown())
		sub_result.add(self.check_files_legacy_empty_state_ok())
		sub_result.add(self.check_non_acp_path_files())
		result.append(sub_result)

		# ----- Additional check_files edge cases
		sub_result = TestResult('check_files Edge cases')
		sub_result.add(self.check_files_modern_empty_state_ok())
		sub_result.add(self.check_files_paths_comma_separated())
		sub_result.add(self.check_files_file_alias())
		sub_result.add(self.check_files_path_is_a_file())
		sub_result.add(self.check_files_invalid_filter_syntax())
		sub_result.add(self.check_files_trailing_backslash())
		sub_result.add(self.check_files_forward_slashes())
		sub_result.add(self.check_files_show_all_flag())
		sub_result.add(self.check_files_glob_in_path())
		result.append(sub_result)

		# ----- Acceptance tests for the new check_single_file command
		sub_result = TestResult('check_single_file')
		sub_result.add(self.check_single_file_ok())
		sub_result.add(self.check_single_file_warning())
		sub_result.add(self.check_single_file_missing())
		sub_result.add(self.check_single_file_no_arg())
		sub_result.add(self.check_single_file_non_acp_path())
		sub_result.add(self.check_single_file_critical())
		sub_result.add(self.check_single_file_age_threshold())
		sub_result.add(self.check_single_file_empty_file())
		sub_result.add(self.check_single_file_against_directory())
		sub_result.add(self.check_single_file_perfdata_shape())
		result.append(sub_result)

		return result

	def install(self):
		conf = self.conf
		conf.set_string('/modules', 'test_disk', 'CheckDisk')
		conf.set_string('/modules', 'pytest', 'PythonScript')
		conf.set_string('/settings/pytest/scripts', 'test_w32file', 'test_w32_file.py')
		conf.save()

	def uninstall(self):
		pass

	def help(self):
		pass

	def init(self, plugin_id):
		self.reg = Registry.get(plugin_id)
		self.core = Core.get(plugin_id)
		self.conf = Settings.get(plugin_id)

	def shutdown(self):
		pass

all_tests = [Win32FileTest()]

def __main__(args):
	install_testcases(all_tests)
	
def init(plugin_id, plugin_alias, script_alias):
	init_testcases(plugin_id, plugin_alias, script_alias, all_tests)

def shutdown():
	shutdown_testcases()
