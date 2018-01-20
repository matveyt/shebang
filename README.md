shebang
=======

Summary
-------
An utility for _MSYS_/_MSYS2_ which allows to execute POSIX shell scripts from the
native Windows command-line.

Description
-----------
_MSYS_ comes with its own port of _bash_ which is able to execute shell scripts. However,
unlike ported binaries, no POSIX shell script can be executed directly from the native
Windows command-line interpretator _cmd_ (or from other Windows programs, such as _FAR_,
etc.).

It's quite easy to come up with the "wrapper script" solution, for example:

    @rem shebang.cmd
    @env MSYSTEM=MINGW64 %~n0 %*

will execute any POSIX script (or native binary) called "shebang" found on PATH. However,
any (incorrectly ported) application will fail if it tries to execute such script by
spawning it directly, which is normal for the POSIX world, but not for Windows, where one
needs to provide an explicit command, such as `cmd /C shebang.cmd`. This is where _shebang_
comes to help.

Building
--------
_Shebang_ was successfully built and tested with _GCC/MinGW 7.2.0_.

There is a _meson_ project script provided with the distribution. You will need
_meson_, _python3_ and _ninja_ to make use of it.

However, `shebang` could be also compiled in the command line. For example,
`gcc -s shebang.c -o shebang.exe -lshlwapi` works just nice.

The additional DEFINEs to control the building process are:

    -D_UNICODE -DUNICODE
    -DNOSTDLIB

`_UNICODE` and `UNICODE` are needed to build the "wide" version of an executable.

`NOSTDLIB` is useful for reducing the size of an executable by linking it with
a custom startup code, such as _nocrt0_.

Using
-----
Simply rename or symlink _shebang_, so its name matches the script you want and put it on PATH.
Now upon execution _shebang_ will find the script on PATH, parse its first (aka "shebang") line,
and execute it with own command-line arguments.
