if get_option('gen_protobuf')
  protoc = find_program('protoc', required: true)
  run_command([protoc, '--c_out=./protobuf', 'netpoker.proto'], check: true)
endif

shared_src = files(
  'client.c',
  'dc_config.c',
  'debug.c',
  'game.c',
  'graphics.c',
  'net.c',
  'protobuf/netpoker.pb-c.c',
  'server.c',
  'util.c',
)

conf = configuration_data()
conf.set_quoted('DEALERSCHOICE_VERSION', meson.project_version())
conf.set_quoted('DEALERSCHOICE_FORMAL_NAME', 'Dealer\'s Choice')
conf.set_quoted('DEALERSCHOICE_NAME', meson.project_name())
conf.set_quoted(
  'DEALERSCHOICE_URL',
  'https://github.com/Dealer-s-Choice/dealers_choice',
)
conf.set_quoted(
  'DEALERSCHOICE_BUGREPORT',
  'https://github.com/Dealer-s-Choice/dealers_choice/issues',
)
conf.set_quoted(
  'DEALERSCHOICE_DATADIR',
  join_paths(get_option('prefix'), get_option('datadir'), 'games', meson.project_name()),
)

configure_file(output: 'config.h', configuration: conf)
