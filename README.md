# quickstream

A software development toolkit for signal processing which uses block
modules.

quickstream is currently being developed on the Debian GNU/Linux 12
(bookworm) with the GNOME desktop.  It was also developed on Ubuntu 22.04
with a XFCE desktop.


### Stability?

Being currently in alpha, things can change.  No git tags yet.

### Very Few Dependencies

libc, pthreads, gcc, and GNUmake.  Very little more. Optionally GTK3, but
kind-of not very flashy without GTK3.  Dependency details are listed below ...


## What is quickstream?

In brief: quickstream is a C code API (application programming interface)
DSO (dynamic shared object) library and several executable programs that
link with that library, and many smaller C code objects (modules) called
blocks.  quickstream steals many great ideas from
[GNU Radio](https://www.gnuradio.org/) but is far smaller.


### quickstreamGUI

![image of a stream graph being run and edited with the quickstream GUI
program](https://repository-images.githubusercontent.com/659916367/85c2fe98-12bf-4e71-87b8-775de99850b4)

The above image shows quickstreamGUI simultaneously running and editing a
quickstream flow graph.


### quickstream

quickstream is also a command-line program that runs "quickstream"
programs.  It comes with a very verbose "--help" option.  It comes with
bash-completion and is a simple interpretor.  The quickstream command-line
program has mostly the same powers as quickstreamGUI but without the GUI.
The two programs quickstreamGUI and quickstream compliment each other.


### libquickstream.so

libquickstream.so is a DSO library that links with quickstreamGUI and
quickstream (command-line program).  libquickstream.so does the heavy
lifting.


### Blocks

Blocks are small pieces of compiled C (or C++) code.

There are chooses as to how to get them into the quickstream environment.
One aspect is whither the block is a separate DSO file or is it built into
libquickstream.so.  Another aspect of a block is whither it is a simple
block or a super block.  Super blocks are similar to GNU Radio hier
(hierarchical) blocks.


## Building and Installing

### Dependences

As noted before, it's currently be developed on Debian GNU/Linux 12 with a
GNOME desktop.  Here's a list of the deb packages that I think are needed
to build quickstream:

~~~
gcc make graphviz imagemagick doxygen wget libgtk-3-dev librtlsdr-dev gnuradio-dev vim-gtk3 gnome-terminal qt6-base-dev qt6-tools-dev qt6-wayland
~~~

Some of these packages are optional, like all the packages after the wget
package.

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
vim-gtk3 gnome-terminal qt6-base-dev qt6-tools-dev \
qt6-wayland
~~~


The following bash script has a good chance to end by running the
quickstreamGUI program.  Please be aware that there are many downloaded
scripts in it that you most certainly do not want to run as a user with
super powers.  Better yet, just step through it looking at the scripts
before you run them. 

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

If you see the GUI now, great.  Try "right click"; I think that there is a
"right click" menu pop-up for every thing you see.  And, left click to
make connections and select/grab blocks.


## Latest News

__February 2024__
I gave a talk at FOSDOM 2024 on quickstream.  So far, I do not know that
anyone has any interest in quickstream, except myself.  I'll continue to
develop it without adding documentation or software releases (git tags).
Having no users has its benefits.

__March 2024__
Current development focus: Started adding optional Qt6 widgets; likely a
replacement to GTK3 in general.  In short: GTK3 is too limited compared to
Qt6 widgets.  Note: adding Qt6 C++ code will increase quickstream package
compile times considerably.  It's not usable yet, but tests show
promise.

