project('shebang', 'c', default_options : \
  ['buildtype=minsize', 'c_std=c11', 'warning_level=3', 'werror=true'])


sources = ['shebang.c']
c_args = ['-fno-ident']
link_args = ['-s', '-lshlwapi', '-Wl,--gc-sections']


if not get_option('ansi')
  c_args += ['-D_UNICODE', '-DUNICODE']
  link_args += '-municode'
endif


if get_option('nostdlib')
  sources += 'nocrt0c.c'
  c_args += ['-DNOSTDLIB', '-DARGV=none']
  link_args += ['-nostdlib', '-lkernel32']
endif


executable('shebang', sources : sources, c_args : c_args, link_args : link_args)
