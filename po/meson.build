intl_dep = []
if get_option('nls') and not platform_is_windows
  inc_intl_dir = []
  inc_args = []
  intl_dep = dependency('intl', required: false)
  if not intl_dep.found()
    if host_sys == 'freebsd' or host_sys == 'openbsd' or host_sys == 'dragonfly'
      inc_args = ['-isystem/usr/local/include']
      intl_dep = cc.find_library(
        'intl',
        has_headers: ['libintl.h'],
        dirs: ['/usr/local/lib'],
        header_args: inc_args,
        required: false,
      )
    elif host_sys == 'netbsd'
      inc_args = ['-isystem/usr/pkg/include']
      intl_dep = cc.find_library(
        'intl',
        has_headers: ['libintl.h'],
        dirs: ['/usr/pkg/lib'],
        header_args: inc_args,
        required: false,
      )
    elif host_sys == 'darwin'
      # include_directories() hard-errors on a path that does not exist, which
      # on a Mac without Homebrew gettext would pre-empt the friendly error
      # below. Only name the dir once it is really there.
      fs = import('fs')
      if fs.is_dir('/opt/homebrew/opt/gettext/include')
        inc_intl_dir = include_directories('/opt/homebrew/opt/gettext/include')
      endif
      intl_dep = cc.find_library(
        'intl',
        dirs: ['/opt/homebrew/opt/gettext/lib'],
        has_headers: ['libintl.h'],
        header_include_directories: inc_intl_dir,
        required: false,
      )
    endif
  endif
  if intl_dep.found()
    shared_inc += inc_intl_dir
    shared_dep += intl_dep
    add_project_arguments(inc_args, language: 'c')
    conf.set('ENABLE_NLS', 1)
    i18n = import('i18n')
    i18n.gettext(
      meson.project_name(),
      args: [
        '--add-comments',
        '--keyword=_',
        '--keyword=N_',
        '--directory=' + meson.project_source_root(),
      ],
    )
  else
    error(
      'libintl not found. Install the gettext development package, or configure with -Dnls=false.',
    )
  endif
endif

