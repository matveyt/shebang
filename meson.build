project('shebang', 'c',
    default_options : ['buildtype=minsize', 'c_std=c99', 'debug=false',
    'warning_level=3', 'werror=true'])

gnu = (meson.get_compiler('c').get_argument_syntax() == 'gcc')

sources = ['shebang.c']
c_args = []
link_args = ['-lshlwapi']

if gnu and not get_option('debug')
    c_args += '-fno-ident'
    link_args += ['-s', '-Wl,--gc-sections']
endif

if not get_option('ansi')
    c_args += ['-D_UNICODE', '-DUNICODE']
    if gnu
        link_args += '-municode'
    endif
endif

if get_option('nostdlib')
    sources += 'nocrt0c.c'
    c_args += ['-DNOSTDLIB', '-DARGV=none']
    link_args += '-lkernel32'
    if gnu
        link_args += '-nostdlib'
    endif
endif

executable(meson.project_name(),
    sources : sources, c_args : c_args, link_args : link_args)
