project('tshotkeytrigger', 'c', version: '0.0.1')

config = configuration_data({
  'version' : meson.project_version(),
})

configure_file(input : 'config.h.in',
  output : 'config.h',
  configuration : config)

fs = import('fs')
fs.copyfile('LICENSE', 'LICENSE')
fs.copyfile('THIRD_PARTY_LICENSES', 'THIRD_PARTY_LICENSES')

libwebsocketsdep = dependency('libwebsockets')
executable('tshotkeytrigger', ['main.c', 'datafile.c'], dependencies: [libwebsocketsdep])

