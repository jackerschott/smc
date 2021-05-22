project('pwn', 'c', version : '1.0', license : 'GPLv3', default_options : ['warning_level=0'])

src_msg = files([
	'msg/main.c',
	'api/api.c',
	'api/parse.c',
	'api/list.c',
])
inc = include_directories('api', 'msg')

dep_jsonc = dependency('json-c')
dep_curl = dependency('libcurl')

executable('mtxmsg', src_msg, include_directories : inc,
	dependencies : [dep_jsonc, dep_curl])
