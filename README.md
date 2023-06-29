# quickstream

A software development toolkit for signal processing which uses block
modules.

quickstream is currently being developed on the Debian GNU/Linux 12
(bookworm) with the GNOME desktop.  It was also developed on Ubuntu 22.04
with a XFCE desktop.


### Things that a ferengi would never say:

quickstream is currently in an [alpha
state](https://en.wikipedia.org/wiki/Software_release_life_cycle).  No one
has used it other than me, the sole author.  There is not a single bug in
it that I know about.  It's complex code, so I know there will be bugs
found when this code hits the wild.  I intend to make quickstream a
[robust](http://www.linfo.org/robust.html) framework.  There are lots of
obvious features missing.  I consider there to be enough functionality
to bring it to this alpha state.  It already does plenty of things that no
other software can do.

Being [free software](https://www.fsf.org/about/what-is-free-software)
quickstream is making this alpha released software available here; and
obviously the idea of alpha/beta and so on will be replaced by git tagged
releases.  I guess that means first git tag will mark a beta release.

### Stability

Being currently in alpha, things can change.  If you don't like the
name of a function in the API, than now it the time to act.  I'm not into
making a lot of branches, but I encourage debate that makes the code
better.

### Dependencies

Very few.  libc, pthreads, gcc, and GNUmake.  Very little more. Optionally
GTK3, but kind-of not very flashy without GTK3.   I'm working on a short
bash command that can build/install and demo it on a standard current
stable Ubuntu and Debian system, but first I need to write this GitHub
page.  I'll separate the parts that need root access.


## What is quickstream?

In brief: quickstream is a C code API (application programming interface)
DSO (dynamic shared object) library and several executable programs that
link with that library, and many smaller C code objects (modules) called
blocks.  quickstream steals many great ideas from GNUradio but also
corrects many of it's short comings.  quickstream can be used to program
SDRs (software defined radios) but that is only one usage domain that
happens to be a very popular research topic lately.  Yes, of course, it
links with C++ code (TODO: need to add tests for that).


### quickstreamGUI

![image of a stream graph being run and edited with the quickstream GUI
program](https://repository-images.githubusercontent.com/659916367/85c2fe98-12bf-4e71-87b8-775de99850b4)

The above image shows quickstreamGUI simultaneously running and editing a
quickstream flow graph.  You can see it is feeding two GNUradio programs
with pipes.

quickstreamGUI is a GTK3 GUI (graphical user interface) C compiled program
that interactively builds and runs stream flow graphs similar to what
GNUradio does, but it runs the flow graphs as it edits the flow graphs.
The quickstreamGUI program is running the compiled flow graph code as
you edit it in the quickstreamGUI program, and not in a forked and
executed program like GNUradio does.  It is hoped that this tighter
coupling between editor and block code will produce faster and more
productive workflow patterns than is possible without this tight
block/editor coupling.

### quickstream

quickstream is also a command-line program that runs "quickstream"
programs.  It come with a "--help" option that is so verbose that we
separated the binary program from the help text.  It comes with
bash-completion and is also in-effect a simple interpretor.  The
quickstream command-line program has mostly the same powers as
quickstreamGUI but without the GUI.  The two programs quickstreamGUI and
quickstream compliment each other.  Many (maybe most) of the 100-plus
quickstream test programs use quickstream (the command-line program).


### libquickstream.so

libquickstream.so is a DSO library that links with quickstreamGUI and
quickstream (command-line program).  libquickstream.so does the heavy
lifting.


### Blocks

Myself having a UNIX programming background, I used to call them
"filters".  UNIX has this idea that small programs that read input and
write output are called filters.  I only adopted the idea of calling
modules "blocks" because GNUradio popularized the term and GNUradio is
popular.

Blocks are small pieces of compiled C (or C++) code.  There are chooses as
to how to get them into the quickstream environment.  One aspect is
whither the block is a separate DSO file or is it built into
libquickstream.so.  Another aspect of a block is whither it is a simple
block or a super block.  Super blocks are in some way similar to GNUradio
hier (hierarchical) blocks, but they play a more pivotal role in a
quickstream user/developer workflow.  There are no block classes to
inherit in quickstream.  The default behaviors of a block are just there
in libquickstream.so.

On boiler plate code: to write a minimum quickstream block takes just an
empty C file.  Yes, you can compile a empty file into a DSO on GNU/Linux.
Zero is my hero.  If you need the block to do something you need to add at
least 5 or so lines of code.  There are no other source files required
to write a block.  The GNU/Linux linker loader system handles finding the
needed symbols in your block, and the GNU/Linux linker loader system
handles isolating the blocks codes so they separate access of common
variable and function names within the running program.  Of course,
writers of built in (libquickstream.so) blocks require more dynamic memory
allocates in the block code, as apposed to having the linker loader doing
the memory allocating and mapping as it does for a DSO block.


## Building and Installing

### Dependences

As noted before, it's currently be developed on Debian GNU/Linux 12 with a
GNOME desktop, so here's a list of the deb packages that I think are
needed to build quickstream:

gcc make graphviz imagemagick doxygen libgtk-3-dev librtlsdr-dev gnuradio-dev

Some of these packages are optional, like the last three -dev packages.

Next download the source from github and cd to the top source directory.

### ./bootstrap

Run ./bootstrap

That will download some files.  No other build step with download files
like this.

### ./configure

Run ./configure

That will generate a make file, "config.make".  You can edit the generated
file to change your installation PREFIX, and other options.


### make

Run "make", or better yet run "make -j 8" to build it faster.

### make install

Run "make install"

Installing quickstream is optional.  The source files are layed out with
the same relative paths as the installed files would be.  The programs all
tend to use this relative path layout to figure out how to run, so it runs
the same in the source directory as it does in the installed directory
tree.  One plus to installing it, is that the bash-completion file will
get installed, making the bash-completion for the quickstream command-line
program automatic; but you can source the bash completion file in the
source directory at share/bash-completion/completions/quickstream

