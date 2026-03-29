subdir('widgets')

if get_option('gen_protobuf')
  protoc = find_program('protoc')
  protoc_gen_c = find_program('protoc-gen-c')
  run_target(
    'gen-proto',
    command: [
      protoc,
      '--plugin=protoc-gen-c=' + protoc_gen_c.full_path(),
      '--c_out=' + meson.current_source_dir() / 'protobuf',
      '--proto_path=' + meson.current_source_dir(),
      meson.current_source_dir() / 'netpoker.proto',
    ],
  )
endif

shared_src = files(
  'client.c',
  'dc_config.c',
  'debug.c',
  'game.c',
  'globals.c',
  'graphics.c',
  'links.c',
  'net.c',
  'protobuf/netpoker.pb-c.c',
  'server.c',
  'ui_widget.c',
  'util.c',
)
