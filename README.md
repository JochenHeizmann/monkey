## About

This repository contains the [Monkey][] compiler, all core [Monkey][] modules
and some additional patches, that **are not** currently part of the official
distribution.

If you want to build something usefull with [Monkey][] you need the mojo module,
which **has not been** placed into the public domain and therefore **is not**
part of this project.

My personal advice? Go and buy the pro version of [Monkey][]! For the small
amount of 100$ you can build what ever you want (including commercial
applications) and you'll support [Mark Sibly][] - one really awesome guy.

## What is Monkey?

Quote from the official [Monkey][] homepage:

> Monkey is a next-generation games programming language that allows you to
create apps on multiple platforms with the greatest of ease.

> Monkey works by translating Monkey code to one of a different number of
languages at compile time - including C++, C#, Java, Javascript and
Actionscript.

> Monkey games can then be run on potentially hundreds of different devices -
including mobile phones, tablets, desktop computers and even videogame
consoles.

> Monkey saves you time and allows you to target your apps at multiple markets
and app stores at once, potentially mutiplying sales several times over.

> So what are you waiting for? Get started with Monkey!

## Naming conventions

### Master branches and all tags

They will always refer to the official [Monkey][] release (minus the mojo
module). There are no files modified or replaced. Plain [Mark Sibly][] code as
distributed by himself.

### Feature branches

All branches starting with `feature/` are new features that extend the compiler
and/or core modules to some extend.

E.g. `feature/ios_screen_orientation` introduce a new compiler flag, for the 
iOS target, that behaves like the official `ANDROID_SCREEN_ORIENTATION`.

**Warning:** Do **not** commit the compiled trans binary!

### Fix branches

They start with `fix/` and simply fix some bugs or make *"stuff like it used to
be"*.

E.g. `fix/ios_armv6_devices` slightly change the iOS project settings to support 
old devices, like the first generation iPod.

**Warning:** Do **not** commit the compiled trans binary!

### User branches

Custom compilation of different features and/or fixes for a specific user
working on this project. This is also the only placed where custom compiled
trans binaries are allowed!

## License

### Master branch

Just read the original README.txt file in the root directory. But here are the
key facts:

* Created by the awesome [Mark Sibly][]
* Placed into the public domain
* No warranty implied; use at your own risk

### All other branches

They contain several small improvements, fixes or new features to the [Monkey][]
compiler and/or core modules and have been placed into the public domain. Key
facts:

* Not created by [Mark Sibly][]
* Placed into the public domain too
* No warranty implied; use at your own risk

## Links

* Blog of [Mark Sibly][]
* [Monkey][] homepage
* [Module Reference][]
* Monkey [Forum][]

  [Mark Sibly]: http://marksibly.blogspot.de/
  [Monkey]: http://www.monkeycoder.co.nz/
  [Module Reference]: http://blitz-wiki.appspot.com/Module_reference
  [Forum]: http://www.monkeycoder.co.nz/Community/_index_.php
