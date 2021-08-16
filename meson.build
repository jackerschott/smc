project('pwn', 'c', version : '1.0', license : 'GPLv3', default_options : ['warning_level=0'])

configdir = meson.source_root() / 'config/'
add_global_arguments('-DCONFIG_DIR="' + configdir  + '"', language : 'c')

src_msg = files([
	'msg/main.c',
	'msg/menu.c',
	'msg/room.c',
	'msg/sync.c',
	'msg/inputline.c',
	'msg/command.c',
	'mtx/mtx.c',
	'mtx/state.c',
	'lib/hjson.c',
	'lib/list.c',
	'lib/hcurs.c'
])
src_rooms = files([
	'rooms/main.c',
	'mtx/mtx.c',
	'mtx/mtx.c',
	'lib/hjson.c',
	'lib/list.c',
])
inc = include_directories('.')

dep_curses = dependency('ncurses')
dep_readline = dependency('readline')
dep_threads = dependency('threads')
dep_curl = dependency('libcurl')
dep_olm = dependency('olm')
dep_jsonc = dependency('json-c')

#executable('mtxmsg', src_msg, include_directories : inc,
#	dependencies : [dep_curses, dep_readline, dep_threads, dep_curl, dep_olm, dep_jsonc])
#executable('mtxrooms', src_rooms, include_directories : inc,
#	dependencies : [dep_jsonc, dep_curl])

#src_enctest = files([
#	'tests/test_encryption.c'
#	''
#])

executable('hashtest', files(['lib/hashtable.c', 'tests/test_hashtable.c']), include_directories : include_directories('.'))
