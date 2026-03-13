if get_option('gen_protobuf')
  protoc = find_program('protoc', required: true)
  run_command([protoc, '--c_out=./protobuf', 'netpoker.proto'], check: true)
endif

shared_src = files(
  'button.c',
  'client.c',
  'dc_config.c',
  'debug.c',
  'game.c',
  'globals.c',
  'graphics.c',
  'indicator.c',
  'links.c',
  'net.c',
  'protobuf/netpoker.pb-c.c',
  'server.c',
  'ui_widget.c',
  'util.c',
)
