shebang
=======

Summary
-------
An utility for *MSYS2/Cygwin* which allows to execute POSIX shell scripts transparently.

Description
-----------
*MSYS* comes with its own port of *bash* which is able to execute shell scripts. However,
unlike ported binaries, no POSIX shell script can be executed directly from the native
Windows command-line interpretator *cmd* (or from other Windows programs, such as *FAR*,
etc.).

It's quite easy to come up with the "wrapper script" solution, for example:

    @rem shebang.cmd
    @env MSYSTEM=MINGW64 %~n0 %*

will execute any POSIX script (or native binary) called "shebang" found on PATH. However,
any (incorrectly ported) application will fail if it tries to execute such script by
spawning it directly, which is normal for the POSIX world, but not for Windows, where one
needs to provide an explicit command, such as `cmd /C shebang.cmd`. This is where
*shebang* comes to help.

Building
--------
*Shebang* was successfully built and tested with *GCC/MinGW 14.1.0*.

Simply invoke *make* to compile.

Using
-----
Rename or symlink *shebang*, so its name matches the script you want and put it on PATH.
Now upon execution *shebang* will find the script on PATH, parse its first (aka
"shebang") line, and execute it with own command-line arguments.
