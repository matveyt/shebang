project('shebang', 'c', default_options : ['buildtype=minsize', 'c_std=c99', \
    'debug=false', 'warning_level=3', 'werror=true'])


cc = meson.get_compiler('c')
gnu = (cc.get_id() == 'gcc')


sources = ['shebang.c']
c_args = []
link_args = []
dependencies = [cc.find_library('shlwapi')]


if not get_option('debug')
    if gnu
        c_args += '-fno-ident'
        link_args += ['-s', '-Wl,--gc-sections']
    endif
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
    dependencies += cc.find_library('kernel32')
    if gnu
        link_args += '-nostdlib'
    endif
endif


executable('shebang', sources : sources, c_args : c_args, link_args : link_args, \
    dependencies : dependencies)
