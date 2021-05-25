project('pwn', 'c', version : '1.0', license : 'GPLv3', default_options : ['warning_level=0'])

configdir = meson.source_root() / 'config/'
add_global_arguments('-DCONFIG_DIR="' + configdir  + '"', language : 'c')

src_msg = files([
	'msg/main.c',
	'api/api.c',
	'api/hjson.c',
	'api/state.c',
	'api/list.c',
])
inc = include_directories('api', 'msg')

dep_jsonc = dependency('json-c')
dep_curl = dependency('libcurl')
dep_ncurses = dependency('ncurses')

executable('mtxmsg', src_msg, include_directories : inc,
	dependencies : [dep_jsonc, dep_curl, dep_ncurses])
