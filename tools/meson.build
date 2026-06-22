# Optional diagnostic CLIs. Built only with -Dtools=true (off by default so
# release/package builds skip them); never installed. gen_keys_doc is NOT here:
# it is build infrastructure (it generates docs/keys.md) and stays wired in the
# top-level meson.build.
if get_option('tools')
  # Query a registry's server list or send a test announce. Headless: the
  # registry protocol lives in libdc_core, so no SDL is needed.
  # Run as `registry-probe list <host> <port>`.
  executable(
    'registry-probe',
    'registry_probe.c',
    dependencies: dc_core_dep,
    install: false,
  )
endif
