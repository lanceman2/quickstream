
# DSO Plugin Qt6 Widgets

I was thinking of fixing the files leaks in the QCoreApplication.  The way
Qt wraps all system calls makes debugging it very difficult.  It makes a
little sense that they wrap "everything" given they want Qt to be ported
to many operating systems.  This "wrapping" method of coding makes it
very difficult to follow the flow of the code; i.e.  you can't just look
for the system calls that you know are being called, and you have to
follow that many layers of wrapper functions only to get function names
that are not in the original source code and not the system calls you
where looking for either.  And you ask yourself "how does this code call a
function that does not exist in the source code"; so it must be in a
dependency library or (less likely) a composed function name symbol (like
from a CPP macro or other script).  Anyway, I gave up for now.  A future
approach would be to run programs with strace and gdb, while adding little
prints in the Qt code.

It would appear that no one has made plugin modules from Qt (Qt6) widgets
before me:
DSO Plugin Qt6 Widgets
https://forum.qt.io/topic/154863/dso-plugin-qt6-widgets/4 The "Qt
community people" seem very friendly, but I can't imagine that the Qt
developers would be open to fixing system resource leaks in the
QCoreApplication object, given I'm the first one to notice this problem.
It would appear that the file leaks just once or twice, but it does not
seem to continue to leak more than that as time goes on, as you create and
destroy QCoreApplication objects.  So it will not stop me from getting my
code to work.  It might be a good idea to look at /proc/PID/maps to see if
QCoreApplication leaks memory.  I'd guess that valgrind would be not so
good for seeing QCoreApplication memory leaks, given there are many
threads involved.

I made the Qt bug ticket:
QApplication leaks files
https://bugreports.qt.io/browse/QTBUG-122824

Link to how to compile Qt6 from source.
https://wiki.qt.io/Building_Qt_6_from_Git
The amazing thing it that I was able to compile/install all of Qt6 from
source with one try.  And so fuck using APT, and using tens of packages
when I can build and install "all" of Qt with one script.

Their (Qt) source seems to be structured well, unlike GTK source code in
which they to not bother to lock down dependencies with hashes for them.  I
gave up trying to build/install GTK4 from source.  They do not publish
unique hashes of compatible library dependences that are needed to build a
given git tag (or other hash thingy).  Your left guessing which git hashes
of the different GTK library packages are needed.  With Qt source all
the required Qt packages are automatically compatible via the
"init-repository" perl script.  I expect that the GTK developers are
segregating the developing of each component library, without regard to
git hashes (or like thing).  It's shit luck that there is any magic combo
of git hashes that work together to build a coherent libgtk-4.so (or
whatever) that depends on libgdk-4.so (??) and the myriad of other
libraries.


