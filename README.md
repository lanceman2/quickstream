# quickstream

A software development toolkit for signal processing which uses block
modules.

quickstream is currently being developed on the Debian GNU/Linux 12
(bookworm) with the GNOME desktop.  It was also developed on Ubuntu 22.04
with a XFCE desktop.  By switching from ubuntu to Debian some issues where
found and fixed.


### Things that a ferengi would never say:

quickstream is currently in an [alpha
state](https://en.wikipedia.org/wiki/Software_release_life_cycle).  No one
has used it other than me, the sole author.  There is not a single bug in
it that I know about.  It's complex code, so I know there will be bugs
found when this code hits the wild.  I intend to make quickstream a
[robust](http://www.linfo.org/robust.html) framework.  There are lots of
obvious features missing, I consider there to be enough functionality to
bring it to this alpha state.  It already does plenty of things that
no other software can do.

Being [free software](https://www.fsf.org/about/what-is-free-software)
quickstream is making this alpha released software available here; and
obviously the idea of alpha/beta and so on will in time depend on git
tags, and will blur into a spectrum of tagged releases.  But, since no one
but me has seen it yet, except me, we can clearly call it alpha.


### Stability

Being in currently in alpha, things can change.  If you don't like the
name of a function in the API, than now it the time to act.  I'm not into
making a lot of branches, but I encourage debate that make the code
better.

### Dependencies

Very few.  libc, pthreads, gcc, and GNUmake.  Very little more. Optionally
GTK3, but kind-of not very flashy without GTK3.   I'm working on a short
bash command that can build/install and demo it on a standard current
stable Ubuntu and Debian system, but first I need to write this GitHub
page.  I'll separate the parts that need root access.  bash is my
friend.


## What is quickstream

In brief: quickstream is a C code API (application programming interface)
DSO (dynamic shared object) library and several executable programs that
link with that library, and many smaller C code objects (modules) called
blocks.  quickstream steals many great ideas from GNUradio but also
corrects many of it's short comings.  quickstream can be used to program
SDRs (software defined radios) but that is only one usage domain that
happens to be a very popular research topic lately.  Yes, of course, it
links with C++ code (TODO: needs to add tests for that).


### quickstreamGUI

![image of a stream graph being run and edited with the quickstream GUI
program](https://lanceman2.github.io/screenshot_00_small.png)

[full size image](https://lanceman2.github.io/screenshot_00.png)


