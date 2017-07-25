# OCamlCreator

Plugin to add OCaml language support to QtCreator IDE.

Based on the source of RubyCreator: http://hugopl.github.io/RubyCreator/


# Note About branches

`master` branch should work with the `master` branch of QtCreator, it may not compile because QtCreator may have changed some API and we had no time to adapt to the changes.

## How to compile

**You need Qt5!!**

If you want to try QtCreator but don't want to have a custom QtCreator compiled just to do that, follow these instructions:

* ./configure.rb
* cd build && make

At the end of the build you will see an error about lack of permissions to move the plugin library to /usr/..., move it by yourself and it's done.

If you pretend to contribute with RubyCreator or already write plugins for QtCreator you probably already have a custom build of QtCreator installed in
a sandbox somewhere in your system, so just call qmake passing QTC_SOURCE and QTC_BUILD variables.
