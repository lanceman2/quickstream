# quickstream

A software development toolkit for signal processing which uses block
modules.

quickstream is currently being developed on the Debian GNU/Linux 12
(bookworm) with the GNOME desktop.  It was also developed on Ubuntu 22.04
with a XFCE desktop.


### Things that a ferengi would never say:

quickstream is currently in an [alpha
state](https://en.wikipedia.org/wiki/Software_release_life_cycle).  I
intend to make quickstream a [robust](http://www.linfo.org/robust.html)
framework.  There are lots of obvious features missing.  I consider there
to be enough functionality to bring it to this alpha state.  It already
does plenty of things that no other software can do.

Being [free software](https://www.fsf.org/about/what-is-free-software)
quickstream is making this alpha released software available here; and
obviously the idea of alpha/beta and so on will be replaced by git tagged
releases.  The first git tag will mark a beta release.  Until than, please
clone the latest in the master branch.  I'll do my best not to cause a
coding regression.


### Stability?

Being currently in alpha, things can change.  If you don't like the
name of a function in the API, than now is the time to act.  I'm not into
making a lot of branches, but I encourage debate that makes the code
better.

### Very Few Dependencies

libc, pthreads, gcc, and GNUmake.  Very little more. Optionally GTK3, but
kind-of not very flashy without GTK3.  Having very few required
dependencies is a good thing.  Dependency details are listed below ...


## What is quickstream?

In brief: quickstream is a C code API (application programming interface)
DSO (dynamic shared object) library and several executable programs that
link with that library, and many smaller C code objects (modules) called
blocks.  quickstream steals many great ideas from GNUradio, but also
corrects many of its short comings.  quickstream can be used to program
SDRs (software defined radios) but that is only one usage domain that
happens to be a very popular research topic lately.  Yes, of course, it
links with C++ code, as all C code does (well almost all).


### quickstreamGUI

![image of a stream graph being run and edited with the quickstream GUI
program](https://repository-images.githubusercontent.com/659916367/85c2fe98-12bf-4e71-87b8-775de99850b4)

The above image shows quickstreamGUI simultaneously running and editing a
quickstream flow graph.  You can see it is feeding two GNUradio programs
with pipes. If this was an animated image you could see the scopes jiggle
their displays.  The two GNUradio programs are running and feeding the
QT_GUI_Sink block from a pipe from the quickstreamGUI process.  The
quickstream GUI editor runs in the same process as the quickstream flow
graph.  The quickstream GUI editor actual runs the blocks while it
loads and connects them.  gnuradio-companion does nothing like this,
it's not designed to work on-the-fly like this.

quickstreamGUI is a GTK3 GUI (graphical user interface) C compiled program
that interactively builds and runs stream flow graphs similar to what
GNUradio does, but it runs the flow graphs as it edits the flow graphs.
The quickstreamGUI program is running the compiled flow graph code as you
edit it in the quickstreamGUI program, and not in a forked and executed
program like gnuradio-companion does.  It is hoped that this tighter
coupling between editor and block code will produce faster and more
productive workflow patterns than is possible without this tight
block/editor coupling; and not just more seg-faults and core dumps (I can
take it).  So, on the counter side, blocks require a higher quality for
use in quickstream, and that may not necessary be a bad thing.  That's the
give and take of it (zeroth order approximation).  The debate will
ensue...


### quickstream

quickstream is also a command-line program that runs "quickstream"
programs.  It comes with a very verbose "--help" option.  It comes with
bash-completion and is a simple interpretor.  The quickstream command-line
program has mostly the same powers as quickstreamGUI but without the GUI.
The two programs quickstreamGUI and quickstream compliment each other.
Both are essential, though quickstreamGUI may steal the show.


### libquickstream.so

libquickstream.so is a DSO library that links with quickstreamGUI and
quickstream (command-line program).  Put simply, libquickstream.so does
the heavy lifting.


### Blocks

Myself having a UNIX programming background, I used to call them
"filters".  UNIX has this idea that small programs that read input and/or
write output are called filters.  I only adopted the idea of calling
modules "blocks" because GNUradio (among other software packages)
popularized the term and GNUradio is popular.

Blocks are small pieces of compiled C (or C++) code.  There are chooses as
to how to get them into the quickstream environment.  One aspect is
whither the block is a separate DSO file or is it built into
libquickstream.so.  Another aspect of a block is whither it is a simple
block or a super block.  Super blocks are in some ways similar to GNUradio
hier (hierarchical) blocks, but they play a more pivotal role in a
quickstream user/developer workflow; i.e. quickstream forces users to
make a super block order to be able to run a flow graph.  That helps to
insure that product stays in the quickstream workflow, as compared to in
the GNUradio workflow where most product is lost to programs outside the
GNUradio workflow.  If you don't know what I mean, try to reproduce a
GNUradio flow graph (native gnuradio-companion file) from a python script
that was generated from gnuradio-companion, or from the C++ code it
generated.


#### On Block Boiler Plate Code

There is none.

There are no block classes to inherit in quickstream.  The default
behaviors of a block are just there in libquickstream.so, and it tracks
all the blocks in a flow graph.  To write a minimum quickstream block
takes just an empty C file.  Yes, you can compile a empty file into a
DSO on GNU/Linux (zero is my hero).  If you need the block to do something
more interesting, you need to add at least 5, or so, lines of code.  There
are no other source files required to make a block in quickstream.  The
GNU/Linux dynamic linker loader system handles finding the needed symbols
(and building namespaces) in your running block, and the GNU/Linux dynamic
linker loader system handles isolating the blocks codes so they separate
access of common variable and function names within the running program;
OOP (object oriented programming) be damned.  C++ class object data would
be redundant information especially for the case of blocks when they are
loaded as a DSO.  The C++ sugar that is added to C can sometimes just
increase boiler plate code, and block writing is a great example of how
C++ bloats otherwise small C codes.  All that being said, you can write
quickstream blocks in C++.

I've written C++ template based DSO loaders with the libdl.so API and I
had to ask myself what the hell am I doing.  Making a loader wrapper code
to make a C++ object for the sake of forcing users to write more code.
It's just adding layers with no added function, just sugar.

Writers of built-in (libquickstream.so) blocks require more dynamic memory
allocations in the block code, as opposed to having the linker loader do
the memory allocating and mappings as it loads a DSO block (nothing to do
with whither it's C or C++).


## Building and Installing

### Dependences

As noted before, it's currently be developed on Debian GNU/Linux 12 with a
GNOME desktop, so here's a list of the deb packages that I think are
needed to build quickstream:

~~~
gcc make graphviz imagemagick doxygen wget libgtk-3-dev librtlsdr-dev gnuradio-dev
~~~

Some of these packages are optional, like the last three -dev packages.

Next download the source from github and cd to the top source directory.

### ./bootstrap

Run
~~~
./bootstrap
~~~

That will download some files.  No other build step will download files
like this.

### ./configure

Run

~~~
./configure
~~~

That will generate a very small make file, "config.make".  You can edit
the generated file to change your installation PREFIX, and other
options.


### make

Run

~~~
make
~~~

or better yet run

~~~
make -j 8
~~~

to build it faster.


### make install

Run

~~~
make install
~~~

Installing quickstream (the package) is optional.  The source files are
laid out with the same relative paths as the installed files would be.
The programs all tend to use this relative path layout to figure out how
to run, so it runs the same in the source directory as it does in the
installed directory tree.  One plus to installing it, is that the
bash-completion file will get installed, making the bash-completion for
the quickstream command-line program automatic; but you can source the
bash completion file in the source directory at
share/bash-completion/completions/quickstream


### 2 Scripts for a Download and Demo

This bash script may be able to install all the prerequisite software packages.
If you find packages missing, you can tell me about it.  I do believe it
really is impossible to make this fool proof given how things can vary.

~~~
sudo apt install \
gcc make graphviz imagemagick doxygen wget \
libgtk-3-dev librtlsdr-dev gnuradio-dev \
vim-gtk3 gnome-terminal
~~~


The following bash script has a good chance to end by running the
quickstreamGNU program.  Please be aware that there are many downloaded
scripts in it that you most certainly do not want to run as a user with
super powers.  Better yet, just step through it looking at the scripts
before you run them.  I present it this way so that it is very concise.
It works on my computer, but can simply fail do to things that you do not
control, like a glitch in the github server.  It runs and presents the
GUI program in under ten seconds on my computer.  The cut and paste
web page formatting was correct too.

~~~
mkdir quickstream_source && \
wget --no-check-certificate \
https://github.com/lanceman2/quickstream/tarball/master -O - | \
tar -xvzf - --strip-components 1  -C quickstream_source && \
cd quickstream_source && \
./bootstrap && \
./configure && \
make -j 6 && \
./bin/quickstreamGUI
~~~

If you see the GUI now, great.  There will be a tutorial link here, but
until that happens try "right click"; I think that there is a "right
click" menu pop-up for every thing you see.  And, left click to make
connections and select/grab blocks.


#### Why Does This Code Compile So Fast?

It's C not C++.  C++ takes 10 to a 100 times longer (yes really!!) to
compile than C.  It's good to keep in mind that compiled C code does not
necessarily run faster than compiled C++ code.

Why does GNUradio take so long to compile?  Answer: It's C++ and a lot of
it.


#### C++ Code In This Package?

I've gotten used to this project compiling very quickly, and adding C++
source code would change that dramatically.  Maybe I'll have C++ in
a separate repository.


#### Other Language Bindings?

The future will bring more.

Currently the quickstream software package comes with two language
bindings, one in the command-line program "quickstream" and one in the GUI
program "quickstreamGUI".


#### Documentation?

I started this project some time ago, documenting as I went along.  I have
found that documenting at an early stage was a total waste of time.  I
have, since starting this project, rewritten the "core" code of this
software package at least three times.  In each rewrite I harvested code
from the last writing, but the interfaces (which I documented) changed
dramatically at each rewrite.  That experience caused me to document less
and less as things progressed.

Documentation is in the future, and likely soon.


#### Turorials?

Maybe first, a youtube video.

Web page tutorial?  Maybe I need more blocks first?


#### Motivation For quickstream

[My issue with GNUradio](https://github.com/gnuradio/gnuradio/issues/3702)

I like coding.  I plan to mix nonlinear dynamics and SDR (software define
radio).


## Features In quickstream that are not in GNUradio

- User level worker thread to block assignment at process run time.
  Blocks can share worker threads.

- A block can be written with one source file of zero length

- Blocks do not require more than one file in order to run, that file
  being the compiled DSO (of course there can be more than one)

- Blocks can be added to a running flow graph

- Blocks can be removed from a running flow graph

- Flow graphs can be saved while they are running and flowing

- Stream flows can pass through a block without a copy.  We call
  it a "pass-through" ring buffers.  In general the stream ring
  buffers are all "pass-through" ring buffers but some have
  zero pass-through ports (non-pass-through case).  The block's
  code must decide they want to be a "pass-through" stream block.
  Looks like GNU radio 4.0 will have "pass-through" streams.

- quickstream uses a similar stream ring buffer mapping to GNUradio but it
  uses a completely different flow pointer advancement model which gives
  blocks control over stream ring buffer read and write sizes.  There are
  no thread synchronization primitives held when block callbacks are
  called; as I imagine GNUradio does the same when "work()" is called.
  Put another way the stream ring buffers are "lock-less" (kind-of
  an obvious requirement).

- quickstream can have loops in the stream flow.  A block may directly
  connect from itself back to itself.

- quickstream will make an effort to never let blocks use an inter-thread
  memory access method that corrupts memory, software frameworks should
  strive to be robust like an operating system

- quickstream has no legacy code holding it back


Note: by "running" we mean that the flow graph process is running.  The
stream flowing is a different kind of running.  If a stream connection is
removed due to a block being removed while the process is running and the
stream is flowing, that stream flow may stop.  We decided that having the
stream flow automatically restart due to block removal (or addition) was
too intrusive.

Keep in mind that some of what is written above may be false given that
the source to GNUradio is a moving target.  It would appear the above
applies to GNU radio version 3.x.x, and GNU radio version 4.x.x is
being written now as I write this.


## Comparing gnuradio-companion Workflow with quickstreamGUI

gnuradio-companion does not "run" with the GNUradio runtime library, and
therefore does not provide a language binding for the GNUradio runtime
library.  By not "run" we mean does not make the flow graph flow.  It
turns out the gnuradio-companion program does dynamically link with the
GNU radio "runtime" library, but never "starts" the flow in the
gnuradion-companion process. You see, when a python interceptor runs it
dynamically loads a ton of dependency DSO libraries, much of which the
python programmer may not be aware of.  See
[LD_DEBUG=file](https://stackoverflow.com/questions/40637303/how-to-see-the-order-of-shared-library-loading)
and
[https://bnikolic.co.uk/blog/linux-ld-debug.html](https://bnikolic.co.uk/blog/linux-ld-debug.html).

The way I see it, gnuradio-companion acts as a preprocessing
intermediary between the GNUradio runtime library and programs that run
with the GNUradio runtime library.  C++ and python programs generated by
gnuradio-companion cannot be used as input that can be read by
gnuradio-companion.  This breaks the loop in a user/developer workflow
and the so said produced files cannot be used (read) by
gnuradio-companion.  Hier blocks generated by gnuradio-companion can be
used as input that can be read by gnuradio-companion; and so, use of hier
blocks can add loops in a user/developer workflow, but this is not the
norm for making end user programs with gnuradio-companion.  Imposing
such a "norm" on the gnuradio-companion user would most likely cause a
strain on the GNU radio user community, given the users would lose
a great deal of programming flexibility if such a norm was imposed by
gnuradio-companion.

quickstreamGUI links and runs with the quickstream runtime library and
does in effect provide a language binding for the quickstream
library (libquickstrea.so).  quickstreamGUI must save super (hier) blocks
in order to save anything.  Hence, quickstreamGUI forces users/developer
to keep generated files in the user/developer quickstreamGUI workflow
cycle.  The idea is that if a user/developer can run a quickstream
program, then they can also edit that program, even if that was not the
intent of the original program developer.  In this way quickstream keeps
all quickstream programs (programs that link with libquickstream.so and
load/run quickstream blocks) in the quickstream development workflow; and
users/developers that do not like to share will be forced to share with
other users/developers (to some extent).  It remains to be seen that
having the quickstreamGUI default to saving super blocks can make a
viable workflow.

Both quickstream command-line program and quickstreamGUI can both be used
in a user/developer workflow that is cyclic, but since quickstream
command-line is not graphical in nature, it can be difficult to see
complex flow graphs with it.  quickstream command-line program does have,
on the fly, graph-viz dot display options which quickstreamGUI also has.


## Development Notes, Present and Future

quickstream simple blocks can do crazy things like start the flow graph.
Simple blocks can even destroy the flow graph that they reside in.  Such
ideas are very foreign to GNU radio; put simply, GNU radio (version 3.x.x)
blocks can't "run" themselves.

In order to talk about the quickstream framework we must define at least
five quickstream user domains:

1. core library writer

2. simple block writer

3. graph builder

4. graph runner

5. end application user

These user domains are in a sense fuzzy, but are still necessary in order
to understand the quickstream software framework.  There are some
"special" simple blocks that can run a flow graph.  Simple blocks are not
allowed to build a graph.  Writing a simple block that runs the flow
graph and also acts as a filter would very likely be a bad thing.  The
order of "graph runner" before "graph builder" may make sense, in that
blocks (simple and super) are assembled by the "graph builder" super
blocks.  Super blocks are not allowed to "run the graph", to do so would
tend to render super blocks as unusable by future users of said super
blocks.

The functions in libquickstream.so that let simple blocks do "graph
runner" type tasks run asynchronously.  Additional simple block "graph
runner" functions will likely be things that change the "scheduler" (a GNU
radio term) like creating thread pools and assigning blocks to threads.  I
know this possible to do, but I'm having a problem with the idea of having
simple blocks that can know about all the simple blocks that are loaded in
the graph.  Additional query interfaces in libquickstream.so for these
special simple blocks that can manage the graph they are in.

Having these special simple blocks that can manage the graph will make the
two programs quickstream command-line and quickstreamGUI be able to
generate super blocks that do everything an end-user application program
needs except load the said super block and then idle (main loop).  We can
add a DSO linker/loader execute function to the super block that does that
for us, and then the super block DSO will be an end user application
program when run with exec(3) and than we let super block DSOs be a
regular super blocks when loaded in the usual libquickstream.so way via
qsGraph_createBlock().   As an example of exec(3) of a library run the
GNU standard C library like so:

~~~
/usr/lib/x86_64-linux-gnu/libc.so.6
~~~

This will print information to stdout.

The libquickstream.so run-time library top-block or graph object has no
graph builder configuration.  libquickstream.so has no idea what a GUI
(graphical user interface) or any GUI API is.  libquickstream.so does have
graph runner configuration.  The GUIs are only in the blocks.  It's a
shame that GUI APIs are not modular.  For example there is a gtk_init()
function that creates an instance of a main loop, but this function
has no corresponding destructor.

