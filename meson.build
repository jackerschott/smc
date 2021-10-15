project('pwn', 'c', version : '0.1', license : 'GPLv3', default_options : ['warning_level=0'])

configdir = meson.source_root() / 'config/'
testconfigdir = meson.source_root() / 'tests/config/'
scriptdir = meson.source_root() / 'scripts/'
add_global_arguments('-DCONFIG_DIR="' + configdir  + '"', language : 'c')
add_global_arguments('-DTEST_CONFIG_DIR="' + testconfigdir + '"', language : 'c')
add_global_arguments('-DSCRIPT_DIR="' + scriptdir + '"', language : 'c')

src_mtx = files([
	'mtx/encryption.c',
	'mtx/devices.c',
	'mtx/mtx.c',
	'mtx/state/apply.c',
	'mtx/state/parse.c',
	'mtx/state/room.c',
	'lib/array.c',
	'lib/hjson.c',
	'lib/list.c',
	'lib/util.c'
])
src_msg = files([
	'msg/command.c',
	'msg/inputline.c',
	'msg/main.c',
	'msg/menu.c',
	'msg/room.c',
	'msg/sync.c',
])
src_tests = files([
	'tests/utils.c'
])
inc = include_directories('.')

dep_curses = dependency('ncurses-without-leaks')
dep_readline = dependency('readline')
dep_threads = dependency('threads')
dep_curl = dependency('libcurl')
dep_olm = dependency('olm')
dep_jsonc = dependency('json-c')

dep_mtx = [dep_curl, dep_olm, dep_jsonc]

#executable('mtxmsg', [src_msg, src_mtx], include_directories : inc,
#	dependencies : [dep_curses, dep_readline, dep_threads, dep_curl, dep_olm, dep_jsonc])

foreach test : ['helloworld']
	src_test = files(['tests' / test / 'test.c'])
	t = executable('test_' + test, [src_test, src_mtx, src_tests],
		include_directories : inc, dependencies : dep_mtx)
	test('hello world', t, is_parallel : false)
endforeach
