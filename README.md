# quickstream

data quickly flows between modules in a directed graph


## Development Status

This is currently vapor-ware.

Current development is on Debian 9 and Ubuntu 20.04 systems.  It's in a
pre-alpha state.  It's design is starting to stabilize.

Currently there is no tutorial, so this software package is pretty useless
for anything but for the research and development of this package.

We are adopting **some** of the [GNU radio](https://gnuradio.org/) terminology.


## About quickstream

quickstream is a flowgraph framework, a digital signal-processing system
that operates within running processes;  in this case processes being work
callback functions, not an OS (operating system) process.  It's internals
do not span across multiple computers, though it's applications may.
quickstream avoids system calls when switching between processing modules
and hence uses shared memory to pass stream data between work modules in
the same OS process.

quickstream technically does not implement a [Kahn process
network](https://en.wikipedia.org/wiki/Kahn_process_networkshttps://en.wikipedia.org/wiki/Kahn_process_networks)
(and nether does GNU radio) since its behavior is not deterministic
because of asynchronous parameter changes, in addition to running on
non-deterministic operating systems.

Unlike [GNU radio](https://gnuradio.org/) quickstream does not use a
synchronous data flow model, that is, in quickstream the stream data is
not restricted to keep constant ratios of input to output between blocks,
as it is in GNU radio.  The stream data flow constraints less
than those imposed by GNUradio.  [Synchronous data
flow](https://en.wikipedia.org/wiki/Synchronous_Data_Flow) is not the only
method that can be used to allow static scheduling and buffering.

The flow scheduler code is not like GNU radio's scheduler.  The run model
lets the user use any number of threads from thread pools which may have 1
to N threads.  All quickstream programs can run with one thread or any
number of threads.  Thread affinity may be set, if needed, for blocks of
the users choosing.  quickstream is expected to use much less computer
system resources than GNU radio in preforming similar tasks. 

quickstream is C code with C++ wrappers.  It consists of the quickstream
runtime library and block module plugins.  The quickstream runtime library
and the block module plugins are all created as DSOs (dynamic shared
objects).  quickstream runtime library includes a super block module
creation tool that makes it possible to create and use block module
plugins that congregate many block module plugins.

quickstream comes with two application builder programs:

  - **quickstream** a program which lets to build flow stream programs
    using the command-line.

  - *quickstreamBuilder* a GUI (graphical user interface) program that
    lets you build and run flow stream programs.  It may resemble the GNU
    Radio Companion to some extent.

quickstream APIs (application programming interfaces) are separated into
the three classes of quickstream use:

  - **quickstream/app.h** for running flow graphs

  - **quickstream/builder.h** for connecting blocks together to make flow
    graphs

  - **quickstream/block.h** for building plugin module blocks

The block module writer with not likely use *app.h*, and the app writer will
likely not use *block.h*.

The quickstream starting point is this web page at
https://github.com/lanceman2/quickstream.git, should it move we wish to
keep that URL current.

We keep the current Doxygen generated manual in html at
https://lanceman2.github.io/quickstream.doc/html/index.html


## Quick start

Install the [required core prerequisite packages](#required-core-prerequisite-packages),
and then run
```console
make
```
in the top source directory.   Then run
```console
make test
```

You do not have to install this package to use all of it's functionality.
See more details on building and installing below.


## Required, core, prerequisite packages

Building and installing quickstream requires the following debian package
prerequisites:

```shell
build-essential
```

## Optional prerequisite packages

You can install the following debian package prerequisites, but they
will are not required to build the core of quickstream:


```shell
apt install
 libudev-dev\
 libfftw3-dev\
 graphviz\
 imagemagick\
 doxygen\
 librtlsdr-dev\
 libasound2-dev\
 python3-dev\
 libssl-dev\
 libuhd-dev
```

If you do want to generate documentation you will need the three packages:
graphviz, imagemagick, and doxygen.   If you install any of these we
recommend installing all three, otherwise some of the make/build scripts
may fail.

If you do not currently have a particular application that you need to
make and/or you are not in the final optimizing of your application, we
recommend just installing all of these packages.  Then you will not have
as much unexpected heart ache, at the cost of installing some packages,
most of which you likely already have if you've got to this point in
life.


## Building and Installing with GNU Autotools

quickstream can (or will) be built and installed using a GNU Autotools
software build system.

If you got the source from a repository run

```console
./bootstrap
```

to add the GNU autotools build files to the source.

Or if you got this source code from a software tarball release change
directory to the top source directory and do not run bootstrap.

Then, from the top source directory run

```console
./configure --prefix=/usr/local/PLACE_TO_INSTALL
```

where you replace */usr/local/PLACE_TO_INSTALL* with a directory where you
will like to have the *quickstream* software package installed.  Then run

```console
make
```

to build the code.  Running *make -j 6* could be much faster.

Then run

```console
make install
```

to install the stream software package in the prefix directory that you
set above.

If you just want to play around with quickstream without installing it,
you can skip the *make install* step.  As we said before, all quickstream
programs can run without being installed.  You might think that it would
be a half-ass way to work on code, but you'd be wrong.  Compilers now have
an option to do "relative path linking" of shared object libraries.  Most
software package developers just don't what this is, and/or how to use
these compiler options effectively.  Relative path shared object library
linking is not that new, it's just not that well known and used yet.  It
will be likely to catch on when software build systems start using it by
default.  Using it is the difference to a program that can run and one the
does not run due to a library linker/loader error.  GNU autotools (I think
libtool) has a work-around hack that makes shell script wrappers that let
run programs that other-wise do not run until you install them.
Pain-in-the-ass.   CMake has a more in-your-face center-of-the-universe
approach to the problem, that I'm not going into here.
Bigger-pain-in-the-ass.  CMake is a bloat monster.  Not UNIX-like.  That
can be okay when it works, but debugging it is so much fun.  It keeps
people employed; right Bill.


### Making a Tarball Release

If you wish to remove all files that are generated from the build scripts
in this package, leaving you with just files that are from the repository
and your added costume files you can run *./RepoClean*.  Do not run
*./RepoClean* if you need a clean tarball form of the package source,
use *make distclean* (after running ./configure) for that.

We use the GNU autotool build system to generate tarball releases.
Tarball releases will contain all files that are kept in the repository,
plus more files.  To make a tarball release, after you finish testing and
editing, edit include/quickstream/app.h changing the version numbers and
so on, then you can run a sequence of programs something like the
following:

If you have not already run this before, run

```console
./bootstrap
```

which will download/generate many files needed for a tarball release.
Then run:

```console
./configure
```

which is famous GNU autotools generated *configure* script for this
package.  Then run:

```console
make dist
```

which should generate the tarball file *quickstream-VERSION.tar.gz* and
other compression format versions of tarball files.


## Building and Installing with quickbuild

quickbuild is the quickstream developers build system.  Without some care
the quickbuild build system and the GNU autotools build system will
conflict with each other.  So why do we have two build systems?:

GNU autotools does not allow you to load dynamic shared object (DSO)
plugin modules without installing them first, whereby making a much longer
compile and test cycle.  The same, more-so, can be said of CMake.  As a
code developer we just want to quickly compile source and run it from the
source directory, without installing anything.  Imposing this requirement
on this project has the added benefit of forcing the source code to have
the same directory structure as the installed files, whereby making it
very easy to understand the file structure of the source code.

The use of quickbuild is for quickstream developers, not users.
quickbuild is just some "GNU make" make files and some bash scripts.  We
added these make files named "Makefile" and other files so that we could
develop, compile and run programs without being forced to install files.
We use "makefile" as the make file for GNU autotools generated make files,
which overrides "Makefile", at least for GNU make.


### quickbuild - quick and easy

Run:

```console
./bootstrap
```

```console
make
```

```console
make install PREFIX=MY_PREFIX
```

where MY_PREFIX is the top installation directory.  That's it,
if you have the needed dependences it should be installed.


### quickbuild with more options

If you got the quickstream source from a repository, or a tarball,
change directory to the top source directory and then run:

```console
./quickbuild
```

which will download the file *quickbuild.make* and generate *config.make*.
Now edit *config.make* to your liking.  Then run:

```console
make
```

to generate (compile) files in the source directory.  When building with
quickbuild, you can run programs from the source directories without
installing.  After running *make*, at your option, run:

```console
make install
```

to install files into the prefix (PREFIX) directory you defined.

Note the *PREFIX* will only be used in the *make install* and is not
required to be set until *make install* runs, so alternatively you can
skip making the *config.make* file and in place of running *make install*
you can run *make install PREFIX=MY_PREFIX*.

If you wish to remove all files that are generated from the build scripts
in this package, leaving you with just files that are from the repository
and your added costume files, you can run *./RepoClean*.  Do not run
*./RepoClean* if you need a clean tarball form of the package source,
use *make distclean* for that, or just keep the tarball around for that.

quickbuild does not have much in the line of options parsing, which is
fine for quickstream developers, but not so good for users; hence GNU
autotools is the preferred build/install method.  The optional dependences
that for not found when building with quickbuild will cause it to exclude
building what it can't automatically, but the GNU autotools method adds
configuration options.


## quickstream is Generic

Use it to process video, audio, radio signals, or any data flow that
requires fast repetitive data transfer between block modules.


## quickstream is Fast

The objective is that quickstream flow graph should process data faster
than any other streaming API (application programming interface).
Only benchmarks can show this to be true.


## No connection types

The quickstream use case is generic, in the way it does not care what kind
of data is being transferred between block stages, in the same way UNIX
pipes don't care what kind of data flows through them.  The types of data
that flows is up to you.  The typing of data flowing between particular
blocks is delegated to a higher protocol layer above quickstream.
quickstream provides generic management for the connecting and running of
filter block streams.  quickstream does not consider ports and channels between
blocks to be typed.


## Restricting block modules leads to user control and runtime optimization

quickstream restricts block interfaces so that it may provide an
abstraction layer so that it may vary how it distributes threads and
block modules at runtime, unlike all other high performance streaming
toolkits.  The worker threads in quickstream are not assigned to a given
block module and migrate to block modules that are in need of workers.
In this way threads do a lot less waiting for work, whereby eliminating
the need for threads that do a lot of sleeping, whereby eliminating lots
of system calls, and also eliminating inter thread contention, at runtime.
Put another way, quickstream will run less threads and keep the threads
busier by letting them migrate among the block modules.  Yes, we knew
you'd ask: "what about locking down cores to threads".  You have the
option using the same code and interfaces to set thread cpu affinity.  The
interface to adjusting thread cpu affinity is at the end user and the
block assembler level; block writers are discouraged from setting thread
cpu affinity, but there nothing to stop it if there is a driving need.


## Pass-through buffers

quickstream provides the buffering between block modules in the flow.  The
filter blocks may copy the input buffer and then send different data to
it's outputs, or if the output buffers are or the same size as the input
buffers, it may modify an input buffer and send it through to an output
without copying any data whereby eliminating an expensive copy
operation.


## Description

quickstream is minimalistic and generic.  It is software not designed for
a particular use case.  It is intended to introduce a software design
pattern more so than a particular software development frame-work; as
such, it may be used as a basis to build a frame-work to write programs
to process audio, video, software defined radio (SDR), or any kind of
digit flow pipe-line.

Interfaces in quickstream are minimalistic.  To make a block you do not
necessarily need to consider data flow connection types.  Connection types
are left in the quickstream user domain.  In the same way that UNIX
pipe-lines don't care what type of data is flowing in the pipe,
quickstream just provides a data streaming utility.  So if you put the
wrong type of data into your pipe-line, you get what you pay for.  The API
(application programming interface) is also minimalistic, in that you do
not need to waste so much time figuring out what functions and/or classes
to use to do a particular task, the choose should be obvious (at this
level).

The intent is to construct a flow stream between blocks.  The blocks do
not necessarily directly concern themselves with their neighboring blocks;
the blocks just read input from input ports and write output to output
ports, not necessarily knowing what is writing to them or what is reading
from them; at least that is the mode of operation at this protocol
(quickstream API) level.  The user may add more structure to that if they
need to.  It's like the other UNIX abstractions like sockets, file
streams, and pipes, in that the type of data in the stream is of no
concern in this quickstream API.

This implementation a "quickstream" program runs as a single process and
an on the fly adjusted number of worker threads.  The user may restrict
the number of worker threads at or below a user specified maximum, which
may be zero.  Having zero worker threads means that the main thread will
run the stream flow.  Having one worker thread means that the one worker
thread, only, will run the stream flow; but in that case the main thread
will be available for other tasks while the stream is flowing/running.


## Terminology

To a very limited extend we follow de facto standard terms from [GNU
radio](https://www.gnuradio.org/).  We avoided using GNU radio terminology
which we found to be confusing and not general enough.


### Graph

The directed graph that data flows in.   Nodes in the graph are called blocks.


#### Stream states

A high level view of the stream state diagram (not to be confused with a
stream flow directed graph) looks like so:

![image of stream simple state](
https://raw.githubusercontent.com/lanceman2/quickstream.doc/master/stateSimple.png)

which is what a high level user will see.  It's like a very simple video
player with the video already selected.  A high level user will not see
the transition though the *pause* on the way to exit, but it is there.

If we wish to see more programming detail the stream state diagram can be
expanded to this:

![image of stream expanded state](
https://raw.githubusercontent.com/lanceman2/quickstream.doc/master/stateExpaned.png)

A quickstream block writer will need to understand the expanded state
diagram.  Because there can be more than one block there must be
intermediate transition states between a *paused* and flowing state, so
that the blocks may learn about their connectivity before flowing, and
so that the block may shutdown if that is needed.

There is only one thread running except in the flow and flush
state.  The flush state is no different than the flow state
except that sources are no longer being feed.

- **wait** is a true do nothing state, a process in paused waiting for
  user input.  This state may be skipped through without waiting if the
  program running non-interactively.  This state, maybe, is needed for
  interactive programs.
- **configure** While in this mode the stream can be configured.  All the
  block modules are loaded, block modules *construct* functions are
  called, and stream block connections are figured out.  Block modules
  can also have their *destroy* functions called and they can be unloaded.
  Reasoning: *construct* functions may be required to be called so that
  constraints that only known by the block are shown to the user, so that
  the user can construct the stream.  The alternative would be to have
  particular block connection constrains exposed separately from outside
  the module dynamic share object (DSO), which would make block module
  management a much larger thing, like most other stream toolkits.  We
  keep it simple at the quickstream programming level.
- **start**: the blocks' start functions are called.  Blocks will see how
  many input and output channels they will have just before they start
  running.  There is only one thread running.  No data is flowing in the
  stream when it is in the start state.
- **flow**: the blocks feed each other and the number of threads running
  depends on the thread partitioning scheme that is being used.  A long
  running program that keeps busy will spend most of its time in the
  *flow* state.
- **flush**: stream sources are no longer being feed, and all other
  non-source blocks are running until all input the data drys up.
- **stop**: the blocks stop functions are being called. There is only one
  thread running.  No data is flowing in the stream when it is in the
  stop state.
- **destroy** in reverse load order, all remaining block modules
  *destroy* functions are called, if they exist, and then they are
  unloaded.
- **return**: no process is running.

The stream may cycle through the flow state loop any number of times,
depending on your use, or it may not go through the flow state at all,
and go from pause to exit; which is a useful case for debugging.


### Block

A block is the modular plugin building block of quickstream which as a
compiled code that has a limited number of "quickstream standard" callback
functions in it.  The smallest number of standard callback functions is
zero.  In quickstream all blocks are treated same, in that there is no top
block.  We adopted this term from GNU radio and it's meaning is similar to
that a block is in GNU radio.  One difference is that in quickstream
blocks only expose a limited set of symbol/function interfaces to the rest
of the blocks.  quickstream used the link/loader system and not C++
constructs to restrict interfaces.  For example a block writer can't so
easily expose public member functions to other blocks, and so memory
between blocks is inherently protected.  Interfaces between blocks is much
more restricted.  Interfaces between blocks are standardized by
quickstream.


### work/flow

In quickstream the flow() function is some-what analogous GNU radio's
work() function.  We say some-what because in quickstream we have other
events and corresponding functions that cause block module function's to
do work.  In quickstream flow() is the work that is done to feed the data
into the stream buffers.  In quickstream blocks can also do work do to
events that are not due to changes in the stream buffers, like file I/O
and OS (operating system) signal events.


### Controller

or controller block.  A controller is plugin block module that does not
have stream data input or stream data output is called a controller block
or controller for short.  The controller has no flow() function.  The
controllers flow-time working functions are not directly caused by
stream buffer flows.


### Filter

or filter block.  A filter is a plugin block module that reads input
and/or writes outputs in the stream via a flow() function.  The analogous
GNUradio function is work().


### Source

or source filter block.  A source is a plugin filter block module that has
no inputs and one or more outputs.  Source block's flow() calls are
triggered by like file I/O and OS (operating system) signal events.


### Sink

or sink filter block.  A sink is a plugin filter block module that has
no outputs and one or more inputs.


### Parameter

A single value or small data structure that can be shared between blocks
via "get" or "set".  The parameters are owned by blocks, such that if the
block that owns a parameter is destroyed than the parameters that it owns
will be destroyed with it.

This implementation of quickstream runs the parameter setting and getting
queues with the same code that queues the stream flow functions.  So
quickstream is at it's core using a simple event-driven architecture.
The parameter modification events use the same queuing codes and data
structures as the stream flow events, but with slightly different queuing
priorities.  We were influenced by ideas from nodeJS.  We were turned off
by Python when we found out by coding with libpython that Python threads
do not run in parallel, WTF.  Don't get me wrong, Python is a far superior
software project than quickstream, but fact is fact.  It begs the
question, how does GNUradio handle stream flows with more than one python
block.  Maybe I'm just a stupid-head.


### Channel

Channel is a connection to a filter from another filter.  For a given
filter running in a stream at a particular time, there are input channels
and output channels.  For that filter running in a given cycle the input
and output channels are numbered from 0 to N-1, where N is the number of
input or output channels.  The filters may decide how many input and
output channels they will work with.


### Shared buffers and Port

Channels connect any number of output ports to one input port.  There can
only be one writing filter at a given channel level.  If there is a
"pass-though" channel, than there can a trailing "pass-through" writer at
each "pass-though" level.  The "pass-through" writer writes behind all the
reading ports in the level above.  Pass-through buffering is a feature not
present in GNU radio (version 3.8).


## OS (operating system) Ports

Debian 9 and Ubuntu 18.04, 20.04


## Similar Software Projects

Most other stream flow like software projects have a much more particular
scope of usage than quickstream.  These software packages present a lot of
the same ideas.  We study them and learn.

- **GNUradio** https://www.gnuradio.org/
- **gstreamer** https://gstreamer.freedesktop.org/
- **csdr** https://github.com/simonyiszk/csdr We like straight forward way
  csdr uses UNIX pipe-line stream to make its flow stream. Clearly not the
  most high performance thing to do, but otherwise love it.  It's what I
  do in my prototyping all the time.  When comparing it with quickstream
  it begs the question could UNIX streams be rewritten with inter-process
  shared memory, without kernel buffer to user buffer transfer.  It'd be
  so much faster than the current standard UNIX pipe-lines.  Maybe another
  day when I have another life-time to burn.


### What quickstream intends to do better

Summary: Run faster with less system resources.  Be simple.

In principle it should be able to run fast.   We let the
operating system do its' thing, and try not to interfere with a task
managing code.   All filters keep processing input until they are blocked by
a slower down-stream filter (clogged), or they are waiting for input from
an up-stream filter (throttled).  In general the clogging and throttling
of the stream flows is determined by the current inter-filter buffering
parameters.

"Simple" filters can share the same thread, and whereby use less system
resources, and thereby run the stream faster by avoiding thread context
switches using less time and memory than running filters in separate
threads (or processes).  When a filter gets more complex we can let the
filter run in it's own thread.

It's simple.  There is no learning curve.  There's just less there.  There
is only one interface for a given quickstream primitive functionality.  No
guessing which class to inherit.  No built-in inter-filter data typing.
We provide just a modular streaming paradigm without a particular end
application use case.  The idea of inter-module data types is not
introduced in the flow() interfaces, that would limit it's use, and can be
considered at a higher software interface layer.

In the future benchmarking will tell.  TODO: Add links here...


## A Typical quickstream Flow Graph

![simple stream state](
https://raw.githubusercontent.com/lanceman2/quickstream.doc/master/simpleFlow.png)

## A detailed quickstream Flow Graph

![complex stream state](
https://raw.githubusercontent.com/lanceman2/quickstream.doc/master/complexFlow.png)


## Other graphviz dot quickstream development images

![job flow and related data structures](
https://raw.githubusercontent.com/lanceman2/quickstream.doc/master/jobFlow.png)



## Developer notes



#### Triggering

  TODO: rewrite this section:

  1. For flow graphs with a single file descriptor use blocking w/r
     without epoll_wait().
  2. At flow-time, if there is no non-fd driven task in the queue then the
     last thread will go to epoll_wait().
  3. For dumb users that do not use qsSetFDPoll() (or whatever it's
     called) then they will be adding blocking calls to their running flow
     graphs flow() functions, it will still work, but performance may
     suffer.
  4. Blocks can trigger themselves work from their other callbacks.


#### Classes of user API usage


   1.  Building simple blocks

       2 modes: 1. declare and 2. callbacks

            Connecting stream ports and parameters is not allowed
            Running callbacks is not allowed


   2.  Building super blocks

       2 modes: 1. declare which loads blocks and connects them
                2. can add run/flow time callback functions
                
                Running callbacks is not allowed


   3. Running

       examples: command-line quickstream and GUI quickstreamBuilder

        declaring blocks is not allowed
        Connecting stream ports and parameters is allowed
        Uses run/flow-time API to run streams and shit


#### other

- Are block stream buffer promises a declarative thing?  No, Blocks do not
  necessarily know how many input and outputs they have until start();
  buffer sizing can depend on constant parameters which can change until
  start(), so big NO.
- quickstream code is written in fairly simple C with very few dependences.
  Currently runtime libraries linked are -lpthread -ldl and -lrt.
- The API (application programming interface) user sees data structures as
  opaque.  They just know that they are pointers to data structures, and
  they do not see elements in the structures.  We don't use typedef, for
  that extra layer of indirection makes it harder to "scope the code".
- The files in the source directly follow the directory structure of the
  installed files.  Files in the source are located in the same relative
  path of files that they are most directly related to in the installed
  paths.  Consequently:
  - you don't need to wonder where source files are,
  - programs can run in the source directory after running "make" without
    installing them,
  - the running programs find files from a relative paths between them,
    the same way as in the installed files as with the files in the
    source,
  - we use the compilers relative linking options to link and run
    programs (libtool may not do this, but quickbuild does), and
  - you can move the whole encapsulated installation (top install
    directory) and everything runs the same.
    - there are not full path strings compiled in the library.
- Environment variables allow users to tell quickstream programs where to
  find users files that are not in or from the quickstream source code.
- The installation prefix directory is not used in the quickstream code,
  only relative paths are needed for running quickstream files to find
  themselves.  On a bad note: One needs to worry about package
  installation schemes that brake the encapsulation of the installed
  files.
- C++ (TODO: RUST) code can link with quickstream.
- The public interfaces are object oriented in a C programming sense.
- The private code is slightly more integrated than the public interfaces
  may suggest.
- We wish to keep this C code small and manageable.
  - Run: *LinesOfCCode* to see the number of lines of
    code; 
- Minimize the use of CPP macros, so that understanding the code is easy
  and not a CPP macro nightmare like many C and C++ APIs.  Every
  programmer knows that excessive use of CPP macros leads to code that is
  not easily understandable in the future after you haven't looked at it
  in a while.
- We tend to make self documenting interfaces with straight forward variable
  arguments that are provide quick understanding, like for example we prefer
  functions like **read(void *buffer, size_t len)** to
  **InputObject->val** and **InputObject->len**.  The read form is self
  documenting where as an input object (or struct) requires that we force
  the user to lookup methods to get to the wrapped buffer data.  We don't
  make abstractions and wrappers unless the added benefit for the user
  outweighs the cost of adding new uncommon abstractions.
- The standard C library is a thing we use.  The GNU/Linux and BSD (OSX)
  operating systems are built on it.  For the most part C++ has not
  replaced it, but has just wrapped it with syntactic sugar.
- The linux kernel and the standard C library may be old but they are
  still king, and used by all programs whither people know it or not.
- If you wish to make a tarball release use the GNU autotool building
  method and run *./configure* and *make dist*.
- There will be no downloading of files in the build step.  Downloading
  may happen in the bootstrap step.  In building from a tarball release
  there will be no downloads or bootstrapping.  Packages that do this
  make the job of distributing software very difficult, and are
  suggestive of poor quality code.
- Files in the source are located in the same relative path of files that
  they are most directly related to in the installed paths.
- All filter modules do not share the global variable space that came from
  the module.  Particular filter modules may be loaded more than once.
  Most module loading systems do not consider this case, but with simple
  filters there is a good chance you may want to load a filter more than
  once, with the same filter plugin in different positions in the stream.
  As a result of considering this case, we have the added feature that the
  filter module writer may choose to dynamically create objects, or make
  them statically.  In either case the data in the modules stays
  accessible only in that module.  Modules that wish to share variables
  with other modules may do so by using other non-filter DSOs (dynamic
  shared objects), basically any other library than the filter DSO.
- quickstream is not OOP (object oriented programming).  We only make
  objects when it is necessary.  Excessive OOP can lead to more complexity
  and bloat, which we want to avoid.
- Don't put stuff in quickstream that does not belong in quickstream.  You
  can always build higher level APIs on top of quickstream.  Or can you?
- If you don't like quickstream don't use it.
- The term "Super Module" is used by SymNet Composer software
  https://www.symetrix.co/  This software has a similar connect DSP
  modules GUI.  Only on Windoz. Looks like this company makes DSP
  hardware.  It's not possible to tell what the software inter-thread
  and/or process module connection methods are without access to it.  All
  the descriptions on the web give no program details.  Like most
  proprietary software they do not describe any inner details as to how it
  works.  We can only guess.  Since it appears to be made for audio and
  vision (AV) DSP it does not have as high a speed requirement like that
  needed for software defined radio (SDR).  It has LUA scripting, which is
  great for the LUA project and quickstream.  LUA is simple and nice, not
  bloated and complex like python.



## Driving concerns and todo list

- Simplicity and performance.

- Is this quickstream idea really like a layer in a protocol stack that
  can be built upon?

    * Sometimes we need to care about the underlying workings that
      facilitate an abstraction layer because we have a need for
      performance.
      - It would appear that current software development trends frowns
        upon this idea.

- Bench marking with other streaming APIs; without which quickstream will
  die of obscurity.

- It looks like a kernel hack is not needed, or is it?  mmap(2) does what
  we need.  NPTL (Native POSIX Threads Library) pthreads rock!

- Add "super filter modules" that load groups of filter modules.

- https://www.valgrind.org/ ya that added to development tests.

- Add a auto-run feature to the GUI editor.  The graph runs and when a
  connection is added or removed it stops just before and starts just
  after automatically.  The reallocation of stream buffers needs to
  be lazy, making it quicker.

- The order in which blocks construct() and start() are called needs to be
  editable when the graph is paused.  The order of stop(), destroy(), and
  unload will be the reverse of this order.


## Things to do before a release

- Check that no extra (unneeded) symbols are exposed in libquickstream.
- Run all tests with compile DEBUG CPP flag set.
- Run all tests with compile DEBUG CPP flag unset.
- Run all tests without compiler flag -g.
- Make doxygen run without any warnings.
- Proof all docs including this file.
- Make the base build build in less than 1 minute (last check is about 10
  seconds).
- change QUICKSTREAM_VERSION in the source in include/quickstream/app.h


## Stupid thoughts

Restricting Interfaces at Different Levels

  We do that to make gains.  So you start by losing, but later you gain.
    - Gain is you get modules
       - Seamless expansion across domains

  UNIX philosophy


# FAQ (not really)

These questions are helpful in understanding how quickstream works:

- Is a read promise needed to run the stream?

  Yes.  Without it you can't know/calculate the limiting writer and reader
  pointer boundaries in the ring buffer.

- What if a block does not fulfill it's input read promises?

  The ring buffer could be overrun by a write pointer.  And so some blocks
  could read a mix of the newer written data and older written data.  But
  before this happens the program will stop running.  Blocks must keep
  their read promises.

- Is a write promise needed to run the stream?

  Yes.  The write promise lengths are needed to calculate the ring buffer
  lengths.  We need both all read promise lengths and all write promise
  lengths that are connecting to a ring buffer, to compute the total ring
  buffer lengths.

- What if a block does not fulfill it's output write promises?

  Without a limiting the length that a writer is allowed to write, read
  pointers can be overrun by a write pointer.  The corruption is the same
  as when a read promise is broken.  But it cannot happen either, the
  program will stop running before that happens.  Blocks must keep their
  output write promises or the program will stop running.

- What a fucked up ring buffer model, GNUradio does it better.

  quickstream lets the blocks and end users have more control over the
  sizes of the ring buffers.  Experiments show that sometimes we can make
  a stream graph run much more efficiently by making the per cycle write
  lengths larger, up to some limit.  Of course it greatly depends on the
  hardware and memory available, but without this feature such an
  optimisation is not possible.  This is a run-time option, so we can
  optimise this on the fly.  I don't think GNUradio can do that.  Yes,
  very fucked up.

- The ring buffers are too large.

  The smallest a ring buffer can be is 2 pages (2*4096 bytes).  Yes, that
  does seem large.  But we have seen that by making the rings buffers even
  larger the stream can sometimes run more efficiently than the case with
  smaller ring buffers.  I think GNUradio has fixed ring buffer sizes,
  so I don't think they can play this game.

- Did you copy the ring buffer idea from GNUradio?

  Yes.  It's a great idea.  Thank you, GNUradio.  We did not copy the
  code, just the idea, but we think we extended it, and twisted it.
  quickstream uses read and write promises to determine the ring buffer
  sizes, and GNUradio does not, as far as we can tell.  As a consequence
  quickstream does not require a synchronised block input and output (n/m)
  scheme, and worker threads can roam between blocks letting the end user
  decide how many worker threads to run, independent of the number of
  blocks in the graph.

- Can you pin down a block to a CPU?

  Yes.  You just assign the block (or blocks) of interest a thread pool
  that has one thread in it, and than set the thread affinity of that
  thread.  That was a big concern and that's how we solved that while
  keeping things very flexible.  A quickstream graph can be run by any
  number of thread pools, with any number of threads in each pool.  Each
  block get assigned to a given thread pool.

- WTK is with the command-line program?

  We like UNIX and UXIX-like ideas.  We make all the graph builder
  accessible through the command-line program before they are accessible
  through the GUI builder.  We worked around the limitation of length of
  argv by adding a simple interpreter that reads stream input and treats
  it like command-line arguments.  So we can read very long
  command-line-like-options.  This also enables test driven development,
  which is the most awesome development thing ever.

- Latest Rant about GNUradio

  Similar to GNUradio, but more generic, simpler, and with more restricted
  interfaces.  It may run flow graphs with any number of threads and with
  varying inter-module ring buffer sizes; leading to optimization
  parameters that do not exist in GNUradio.   You can make a block plug-in
  module with just one short C or C++ file.  GNUradio requires lots of
  files and your time to make even a small module.  quickstream does not
  depend on bloat-ware packages, like libboost; as GNUradio does.
  quickstream will strive to be robust, were-as GNUradio is not, and will
  never be, given the large amount of legacy code that would be needed to
  be changed in order to be robust.  The basic design of GNUradio is not
  robust: it allows modules to share variables between different threads,
  and does not restrict interfaces between modules.  In effect, it has
  very little imposed inter-module  structure.  It just has inter-module
  stream flow control with all other inter-module interactions as an
  ad-hoc after-thought.

  quickstream strives to be a robust framework for building multi-threaded
  programs built from modules, called blocks.  Stream data quickly flows
  between blocks using ring buffers.  Control data is passes between
  blocks using function callbacks.  quickstream's basic design is
  minimalistic, so much so that, it had to be completely rewritten three
  times.  The idea that you can build the "under-ware" later, or we can
  abstract it out and rewrite later, are computer science and "pointy
  haired manager" fallacies which do not apply to scope-able minimalistic
  code.  Fore-site is not always possible and "Oh shit, we need a new
  mechanism for that" can happen.  We choose not to let bloat be the
  workaround for lack of fore-site.


- quickstream lack of stream data types

  The basic argument is that the choise of stream data type will in most
  cases be hard coded into the block C/C++ compiled code; so when the user
  chooses a different stream data type (in most cases) they are
  effectively choosing a different compiled block.  Though the user is
  blinded to that in the GNUradio companion (and maybe in the python
  wrappers); at the block writers level they are different blocks.  This
  happens because basic functions like sin(3), sinf(3), and sinl(3) are
  different functions in the standard math library, and there are many
  more examples.  It does not matter that a block writter uses a C++
  wrapper that make them look like the same functions, they are not in the
  compiled code.  Template and/or MACRO coding does not exist in
  compiled code.  So GNUradio is in this way adding a feature the highest
  level interface to the block where you are selecting the type of stream
  data for a block, but under the covers it really selecting between
  completely different blocks.  Because of this their are some things that
  the user can't do.  Any feature is always a constraint.  In the case of
  GNUradio it is tending to be constrainted to be used for software
  defined radio, because of this stream typing feature.  Actually it's
  worst than that, GNUradio is constraining software defined radio to only
  be arrays of simple types transfered between modules.

  quickstream does not add this stream data typing feature.  The blocks
  with different stream data types are different blocks.  This is also
  true of GNUradio, but GNUradio tends to hind this artifact from high
  level users.  For a naive user that can be a good thing.  It maybe
  that quickstream is not for naive users.  quickstream is also not
  restricted to use in just software defined radio.

  For complex stream protcols like video compression streams and most
  stream protocols, the stream data types in GNUradio are not so useful.
  Most stream-like protcols do not have only streams of arrays of simple
  types (like float or complex arrays);  stream protocols like TCP, UDP,
  HTTP, RTCP, and so on, will always be bytes in GNUradio, bytes being the
  type that is the most flexible.

  Another thing that seems to have come about from GNUradio's idea of
  stream data being a series of arrays of simple types is that the "stream
  data flow mode" of GNUradio streams is always "quasi-syncronous", with
  N/M flow rate model.  This is not the case for quickstream.
  quickstream streams flow asynchronously with boundaries imposed by block
  read and write byte length promises.  The promise lengths are fixed
  while the streams are flowing.  quickstream stream data has no types by
  design and in same way that most stream-like protocols, like TCP, have
  type that is carried in the stream; it's just bytes and bits.  GNUradio
  has types as a fundimental part of the design of what stream data is.
  In quickstream connected blocks do not need to keep a relative flow
  rate between them, the flow rates are not even a consideration; the
  connected block are just constrained to not override each other.


# TODO


- *Fd triggers*:  Currently any blocking system call that reads and writes
  will block and wait in the worker thread that is assigned.  With *Fd
  triggers* the thread will not get assigned until epoll_wait() shows the
  fd to be "ready".  Performance may be improved for some use cases.  This
  will be using the standard UNIX-like non-blocking I/O muliplexing
  programing model.  A thing that does not exist on Windos, because not
  all resources can be made into a file.  This would enable users to use
  timerfd_create(2) without sacrificing a worker thread.  We would need to
  use an eventfd(2) to force epoll_wait() to return when it would
  otherwise block forever, like in cases when the determination of when to
  stop is not gotten from a file descriptor that the trigger is for.



