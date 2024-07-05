
# DSO Plugin Qt6 Widgets

I was thinking of fixing the file leaks in the QCoreApplication.  The way
Qt wraps all system calls makes debugging it very difficult.  It makes a
little sense that they wrap "everything" given they want Qt to be ported
to many operating systems.  This "wrapping" method of coding makes it very
difficult to follow the flow of the code; i.e.  you can't just look for
the system calls that you know are being called, and you have to follow
the many layers of wrapper functions only to get function names that are
not in the original source code and not the system calls you where looking
for either.  And you ask yourself "how does this code call a function that
does not exist in the source code"; so it must be in a dependency library
or (less likely) a composed function name symbol (like from a CPP macro or
other script).  Anyway, I gave up for now.  A future approach would be to
run programs with strace and gdb, while adding little prints in the Qt
code.

It would appear that no one has made plugin modules from Qt (Qt6) widgets
before me:
DSO Plugin Qt6 Widgets
https://forum.qt.io/topic/154863/dso-plugin-qt6-widgets/4 The "Qt
community people" seem very friendly, but I can't imagine that the Qt
developers would be open to fixing system resource leaks in the
QCoreApplication object, given I'm the first one to notice this problem.
It would appear that the file leaks just once or twice, but it does not
seem to continue to leak more than that as time goes on, as you create and
destroy QCoreApplication objects.  This will not stop me from getting my
code to work.  It might be a good idea to look at /proc/PID/maps to see if
QCoreApplication leaks memory.  I'd guess that valgrind would be not so
good for seeing QCoreApplication memory leaks, given there are many
threads involved, and multi-threaded program debugging with valgrind I
have found to be not a useful idea.

I made the Qt bug ticket:
QApplication leaks files
https://bugreports.qt.io/browse/QTBUG-122824
It's a basic design flaw that they are not going to fix.  No one but
me sees a use case for destroying system resources for the main loop
Qt object, QCoreApplication (and family of objects).  I tried and failed
to get the Qt developers to fix their code.

Link to how to compile Qt6 from source.
https://wiki.qt.io/Building_Qt_6_from_Git
The amazing thing it that I was able to compile/install all of Qt6 from
source with one try.  And so fuck using APT (debian APT), and using tens
of packages when I can build and install "all" of Qt with one short bash
script:
https://raw.githubusercontent.com/lanceman2/small_utils/master/070_installScripts/qt6


Their (Qt) source seems to be structured well, unlike GTK source code in
which they to not bother to lock down build dependencies with hashes for
them, leaving the builder guessing what are compatible package releases
(git hashes and the like).  I gave up trying to build/install GTK4 from
source.  They do not publish unique hashes of compatible library
dependences that are needed to build a given git tag (or other hash
thingy).  Your left guessing which git hashes of the different GTK library
packages are needed.  With Qt source all the required Qt packages are
automatically compatible via the "init-repository" perl script.  I expect
that the GTK developers are segregating the developing of each component
library, without regard to git hashes (or like thing).  It's shit luck
that there is any magic combo of git hashes that work together to build a
coherent libgtk-4.so (or whatever) that depends on libgdk-4.so (??) and
the myriad of other libraries.


# Big Differences Between GNUradio v3 and quickstream

I did some odd things that I was thinking will not be understood by anyone
for a long time.  Pretty much insane shit.  You'll need to free your mind
or you will just dismiss this as nonsense.  I've tended to remove the
constraints of OOP (object oriented programming) which tend to force users
(block writers) to write more boiler-plate code.

- *block objects* GNUradio blocks inherit block base classes.  quickstream
  block objects are implicit and the block writer does not explicitly
  create them.  This seems to make it so the block code for a quickstream
  block is generally much smaller than for a comparable GNUradio block.
  The smallest source code for a quickstream block is a C file of zero
  length.  The Linux linker-loader-compiler system lets you make DSO
  (dynamic shared object) blocks from zero length files.  FYI, quickstream
  also has builtin blocks that are created from libquickstream.so and use
  no separate block DSO.  I don't know for sure if GNUradio has builtin
  blocks (I don't care anyway).

- *build/run/save on the fly* The quickstream program using the "runtime"
  library, libquickstream.so, can run the flow graph at the same time as
  it edits and/or saves the flow graph.  Put another way: the running flow
  graph process is also a flow graph process editor.  And yet another way:
  a running quickstream process can dump itself while the stream is
  flowing.  This idea does not exist in GNUradio v3.  The program
  gnuradio-companion process does not run flow graphs as it edits them.
  The GNUradio runtime library does not have any kind of flow graph save
  function in it.

All the stuff in slide 3 in this talk
https://fosdem.org/2024/schedule/event/fosdem-2024-1646-quickstream-a-new-sdr-framework/
GNUradio v3 does not do.  As I said in the talk, GNUradio v4 may have more
features than GNUradio v3.

I'm under the impression that this talk was not well received.  I went to
Brussels (FOSDEM 2024) for nothing; maybe.  The one benefit of me giving
this talk is that it will now be pretty hard for people, other than
myself, to take credit for my work.  I think FOSDEM talks could in many
ways be better than refereed papers.

GNUradio v4 will be a complete rewrite of 
https://wiki.gnuradio.org/index.php?title=GNU_Radio_4.0

It remains to be seen whither of not the "quickstream scheduler" (just a
section of complex C code), as GNUradio people might call it, is robust
and performant.


## On Minimum Complete Set of Inter-block Communication Methods

I made up this term: "Minimum Complete Set of Inter-block Communication
Methods".  I tend to think that adding redundant Inter-block Communication
Methods to the "core" flow graph API can tend to be counter productive,
and can add burden to all levels of users of the API from API developer
all the way up to end application user.  This is a "less is more" thing;
or simple is better.


### Inter-block Communication Methods In GNU-radio Runtime Library


This is likely incoherent for must readers given that there is a
lack of context.  I didn't want to write a book.  To some level this is
speculation; but not all, I have changed block code to verify some of
it.

I can't say that all the Inter-block Communication Methods In GNU-radio
are clear to me, and I'd also say it's also not clear to the GNU-radio
developers what is the full list of Inter-block Communication Methods In
the GNU-radio "runtime", as they call it.  I have asked them in person and
never got a straight answer.  It's hard to say what the Inter-block
Communication Methods In GNU-radio version 4.0 given there is no tagged
release yet, it's clearly in flux, and I'm not involved in it's
development.  I made the following list, but this list never got a clear
confirmation from the GNU-radio developers (version 3 or 4).  For the
outside user, there's not much point in listing the Inter-block
Communication Methods In GNU-radio version 4, given there's no git tagged
release yet.  I directly (in person) asked for such a list and got no
answer.


For GNUradio version 3:

- **stream** The main API interface to the block writer is called work().

- **variable** https://wiki.gnuradio.org/index.php/Variable
  Has a design flaw (bug) such that the "variable" memory can be
  corrupted: https://github.com/gnuradio/gnuradio/issues/3702 It's a cool
  idea having variable values that are shared between blocks but it lacks
  access restrictions for the block writer user of variables that keep
  memory from being corrupted.  Most computer programmer do not understand
  what it means to share memory between threads, what inter-thread
  "atomic" operations are, and how thread synchronization functions can
  help in sharing memory between threads.  I say atomic in quotes because
  it's yet another one of those heavily overloaded terms.  Pick the
  "atomic" meaning that works for this case; I'm keeping this short.

  See link:
  https://wiki.gnuradio.org/index.php?title=Runtime_Updating_Variables A
  high level GNUradio usage can exploit this design flaw that I speak of.
  Using just the small flow graph created with GUI (graphical user
  interface) program gnuradio-companion, but any data corruption will not
  be easy to see given that use of a corrupted variable is not propagated
  to any future time steps (for this flow graph); glitches (corrupted
  variable values) are not propagated to future time steps in this case.
  If you insert code into the blocks you can see the glitches, but then
  again such code insertion methods are beyond the comprehension of the
  current gnu-radio developers.

  I've tried to make a simple flow graph that shows this "value glitch"
  without changing any of the compiled block code, but as best I can tell
  the MP to stream data converter blocks are broken, and I suspect it's
  not so easy to do due to the so called "gnuradio scheduler" code not
  working well given that the stream flow is driving all the other
  inter-block communication methods even when there are blocks which
  have continuous MP message generation and no stream input or output.

- **MP** message passing: This used to be called MPI in GNUradio version
  3.x.x (pick the x's that work).  In common usage, MP requires block
  writer to make a key/value parser in the block code.  This seem to me
  to be also called PDU which I'm looking at in a GNUradio documentation
  page but I see no expansion of this PDU acronym.  I think there is a
  page that expands it somewhere.

- **control parameters** are a little like quickstream setter and getter
  control parameters.  The use of this inter-block communication method
  seems to be discouraged.  The current GNUradio 3 documentation
  "Beginner Tutorials" does not mention control parameters.


### User to Block Communication Methods In GNU-radio (v3) Runtime Library

- **parameter** An argument passed to a block C++ constructor or other
  block function (?).  Paratmeters are for the user that is writing the
  flow graph code, though they could be pushed to higher level users
  using **configuration**.


## Inter-block (and other) Communication Methods in the quickstream Runtime Library API (application programming interface)

I keep rethinking what these interfaces should be, and hence I still have
not made a (non-alpha) git tagged release of quickstream; and consequently
have not written a lot of quickstream core blocks (blocks that come with
the package).  As of Jun 10 EDT 2024, I've done at least three complete
rewrites of the quickstream software package, well all but some of the
more modular parts.

This being my personal notes I'm not making this have complete and
coherent description (I don't care), as would be the case if quickstream
had more than one user (me).  If you need a complete and coherent
description you may have to look at the source code to libquickstream.so
and quickstream.h.

In quickstream we have interfaces to the following:

- **stream** input and output.  A lot of the time the stream data between
  blocks can to though of as a time series of values for one or more
  degrees of freedom for a model the flow graph user is simulating; but
  the core of quickstream does not define particular stream data types.
  There could be stream typing on top of the core of quickstream.

- **control parameters** setters (input) and getters (output).  I note
  that a setter or getter "control parameter" could be replaced by a
  stream input or output respectively, but that would waste a lot of
  mmap(2)-ed memory.  The data transfer rate between block's control
  parameters is at least an order of magnitude slower than that of stream
  data.  Hence, like the word "parameter" implies just about constant
  compared a value in a stream.  It up to the blocks to know how to
  interpret what a given control parameters is.

- **inter-block callbacks** For block that know about (and/or depend on)
  other blocks.

## User To Block Communication Methods In The quickstream Runtime Library API

- **block configure** This maybe needs to be expanded.


## Other quickstream Runtime Library Notes

I keep thinking that stream run ("start" and "stop") should not be so
special; given that currently control parameters are running/working so
long as the block exists in a flow graph (nothing like GNUradio).  The run
command calls start() (and stop()) (if they exist) for all the blocks in
the flow graph; that is when run or stop is called on the flow graph.

One can think that stream run (start()) is inherently special because
it queues up stream source jobs which may have no other way to know when
to start stream flow.

Blocks with stream output and no stream input are stream source blocks.
For quickstream the idea of stream source block and sink block is not
inherit to just blocks with only output stream ports connected for a
source or just blocks with only input stream ports connected for a sink.
There is nothing that requires a stream source block have just stream
output and no stream input.  One can write stream source blocks that have
stream input so long as it has stream output; such a source block could be
stimulated to run its' stream flow without requiring stream input.  I may
need to change some code to add the idea of source blocks with both
stream input and output.  The "source blocks" will just be blocks that
like to be "poked" (queue stream jobs) any time the stream starts (run).
The "source blocks" will just tell quickstream that they are "source
blocks" if they are source blocks that have stream input.  If you are a
GNUradio v3 guy, this idea may have just made your head explode, but keep
in mind quickstream streams can have loops, so quickstream uses a very
different stream flow model (scheduler code in GNUradio terms).


## Defining the quickstream block stream interface functions

All the block stream functions are start(), flow(), flush(), and stop().
All these functions except flow() are optional.  If there is no flow()
than this block will not be using the stream, but it can still have
start() and stop() functions.  Source blocks are defined as blocks with
stream output that say they are stream sources, or the blocks that have no
stream input and have stream output are automatically stream source
blocks.  Stream source blocks have stream jobs queued after start() for all
blocks in the flow graph.

While the flow graph stream is flowing "sources" are special in that their
stream jobs will be queued any time the output streams are not clogged.
Any block and especially source blocks may initiate a flow stop of the
stream flow by returning 1 from their flow() function.

## On Stream Flow Running

Try having source (simple) blocks start the stream flow.  One thinks of
the source simple block as being "activated" and it knows it; and the
graph builder that loads the simple source block knows that it is a source
block.  The "graph runner" does not need to necessarily know this, though
it make know it as an emergent property from a super block (graph) that
contains the source blocks.

This brings about the
question: how do we start the stream flow for many source blocks at the
same time?  

Lets say the graph runner interface to this "stream start" is to issue
a block configure command on the source block.  Then in order to issue
this "stream start" to all source blocks in the graph we need to have
a mechanism to tie together many "stream starts" in many simple blocks.
The "graph builder" would make the "stream start" connections.  Maybe the
answer is to use a bool type control parameter setter.  That would lead to
defining a "special" bool type control parameter setter in the case where
the simple block defines itself as a "stream source".  The simple blocks
setter callback would issue a asynchronous stream start command.  That
would be the first "auto-generated block port"; that is having a simple
block declare itself as a stream source will make it so that that block
has a setter with a predefined callback function.

There may be other "auto-generated block ports" like:

- **stream count** a (control parameter) getter that counts bytes of data
  from a simple blocks stream input or output.  This seems to be a
  property a block that is set from outside the block by the super block
  (or graph) that contains the simple block.  The "stream source" property
  of a simple block is defined from within (not outside) the simple block.
  Another property of this "stream count" is that it may be "tagged" with
  a point in the stream that it is measuring.

## On Unconneced Control Parameters

We need to not call getter callbacks if there is nothing connected to the
getter port.  The issue is that the block with the getter calls the push()
function, and so how can we make the unconnected push() as minimal as
possible?   Maybe make getter push() an inline function or CPP macro
function that short circuits when there is not a connection to it's port;
but wait there's more, don't we need to store the new value for future
setter connections?  Do we need an optional "on connect" block callback
for each getter?

## Port Cabling and Patch Panels

What to do when we have simple (or super?) blocks that have hundreds of
control parameters.  A good and obvious example is an oscilloscope.  The
GUI (graphical user interface) would not be so usable one hundred plus
setter ports.

In the code we already have a very fast dictionary for each of the four
types of ports (setter, getter, input, output).  For the case of the GUI
making a connection we can made with a patch panel GUI using the port
names and port descriptions, that will be represented as a single port
to port connection when the patch panel is not displayed.  A patch
panel is property of a super block???  Extend the connection alias idea
making "connection aliases" as a singular case of patch panel port.  This
also necessitates (??) generalizing the port connection API (application
programming interface) in libquickstream.so.

### Patch Panel GUI

How about a GUI that takes the list of all selected blocks and connects
any port "from" the list of all selected blocks "to" the list of all
selected blocks.  It can use the "can connect" function to highlight
ports after one port it clicked on.

### Port Plug Groups Aliases

Extending the port alias idea, port aliases in super blocks (and graphs)
can be constructed.  The connection rules for port plug groups could be
interesting; telling when two plug groups can be connected; kind of like
real physical plugs with sexless, male and female, or combo, plug types.

### Block Port GUI Locations

The current GTK3 program quickstreamGUI shows 4 kinds of ports on 4
different sides of the block.  That will not work when we have an
arbitrary number of "kinds" of ports by adding port plug group aliases.
We could just have all the ports GUI position be adjustable, and not
have just the ports of a setter, getter, input, or output (4 port types)
type on each side of the block.  I.e., we need to be able to move single
ports at a time, without moving all of that type.  So we'll need to
add more GUI (port geometry) state to saved block.  Such data is not
necessary for the functioning of the block without the GUI program.

## quickstream Users

There is just one quickstream user.

Until someone emails me, or I see evidence of lots of real human
downloads, I'll assume there are no users of quickstream and I'll not
worry about breaking someone else's code.  I'm guessing that until
quickstream is used by a popular end user application it will not catch
on no matter if it out performs any competing software.  So, given that,
I'll have to be the one to write the popular end user application that
uses quickstream.


