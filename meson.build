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
	'api/api.c',
	'api/state.c',
	'lib/hjson.c',
	'lib/list.c',
	'lib/hcurs.c'
])
src_rooms = files([
	'rooms/main.c',
	'api/api.c',
	'api/state.c',
	'lib/hjson.c',
	'lib/list.c',
])
inc = include_directories('.')

dep_jsonc = dependency('json-c')
dep_curl = dependency('libcurl')
dep_termbox = dependency('ncurses')
dep_readline = dependency('readline')
dep_threads = dependency('threads')

executable('mtxmsg', src_msg, include_directories : inc,
	dependencies : [dep_jsonc, dep_curl, dep_termbox, dep_readline, dep_threads])
#executable('mtxrooms', src_rooms, include_directories : inc,
#	dependencies : [dep_jsonc, dep_curl])
