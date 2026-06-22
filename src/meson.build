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
      meson.current_source_dir() / 'dc_protocol.proto',
      meson.current_source_dir() / 'dc_registry.proto',
    ],
  )
endif

# Core sources kept at src/ root (each area subdir defines its own *_src below).
core_root_src = files(
  'cli.c',
  'dc_config.c',
  'dc_time.c',
  'debug.c',
  'globals.c',
  'protobuf/dc_protocol.pb-c.c',
  'protobuf/dc_registry.pb-c.c',
  'util.c',
)

subdir('net')
subdir('game')
subdir('server')
subdir('ui')

# Sources shared by the GUI client and the bot (the headless core).
common_src = core_root_src + net_src + game_src + server_src

# GUI-only sources: everything that pulls in SDL2_ttf or SDL2_image, kept out
# of the bot so its binary stays free of those libraries.
gui_src = ui_src

subdir('deckhandler')
shared_dep += deckhandler_dep
subdir('pokeval')
shared_dep += pokeval_dep
