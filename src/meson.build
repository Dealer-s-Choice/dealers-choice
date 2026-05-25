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
  'layout.c',
  'links.c',
  'net.c',
  'protobuf/netpoker.pb-c.c',
  'server.c',
  'style.c',
  'ui_widget.c',
  'util.c',
)

# Subset of shared_src that the bot needs — excludes everything that pulls
# in SDL2_ttf or SDL2_image so the bot binary stays free of those libraries.
bot_src = files(
  'dc_config.c',
  'debug.c',
  'game.c',
  'globals.c',
  'net.c',
  'protobuf/netpoker.pb-c.c',
  'server.c',
  'util.c',
)

subdir('deckhandler')
shared_dep += deckhandler_dep
subdir('pokeval')
shared_dep += pokeval_dep
