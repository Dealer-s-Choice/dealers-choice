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

# Sources shared by the GUI client and the bot.
common_src = files(
  'dc_config.c',
  'debug.c',
  'game.c',
  'globals.c',
  'lan_discovery.c',
  'net.c',
  'protobuf/netpoker.pb-c.c',
  'server.c',
  'util.c',
)

# GUI-only sources: everything that pulls in SDL2_ttf or SDL2_image, kept out
# of the bot so its binary stays free of those libraries.
gui_src = files(
  'client.c',
  'graphics.c',
  'hotkeys.c',
  'layout.c',
  'links.c',
  'style.c',
  'ui_widget.c',
)

shared_src = common_src + gui_src
bot_src = common_src

subdir('deckhandler')
shared_dep += deckhandler_dep
subdir('pokeval')
shared_dep += pokeval_dep
