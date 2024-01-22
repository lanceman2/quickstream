#Tests

This directory and it's sub-directories contains tests that we use in an
effort to keep quickstream bug free and good code.

There are two super test programs that run a large number of sub-tests.


##./run_tests

The **./run_tests** program is the quick test that runs a large number of
programs that finish quickly.  We require that any sub-program tests added
to this program do not slow it down noticeably.  This program takes less
than 1 second to run on a fast computer.


##./valgrind_run_tests

The **./valgrind_run_tests** uses and requires Valgrind be installed in
your run PATH.  Because this uses Valgrind it's a little slow to run.  It
may take a minute to run.  It runs most of the tests that are in
**./run_tests**.


##Adding Tests

Some of the programs in this directory are justs tests run to aid
development, and may not run in the two super test programs, because they
are just testing for miss-information in things like, for example, wrong
answers from stackoverflow.  There is plenty of wrong information on
stackoverflow; even top rated answers.  stackoverflow is generally good
information at least 99 (likely more) percent of the time.

If you would like to contribute a test program, there are some standards
that need to be followed:

 - Tests are just programs that run and return exit status 0 on success,
   123 when the test is skipped, and any other non zero exit status for
   failure.

 - No test that runs in the super test programs shall be slow.  You can
   add slow to finish tests to this directory, just don't let them be run
   in the super tests by default.  A slow test takes longer than one
   tenth of a second to run on a fast computer.

 - Tests that depend on optional dependencies need to automatically skipped
   being run in the super tests if the optional dependencies are not
   accessible.  An easy skip check may be just checking that a particular
   quickstream target file doe not exist in this source/build directory
   tree.  This test automatic skipping can be used to deal with system
   portability, so tests can be skipped due to lack of system support.

 - Before you add a quick running test to the repository you must run it
   successfully many times, say about 10,000 times using a bash while
   loop.  And given that it is a quick running test it should only take
   about 100 seconds to run it 10,000 times.  Given that the threads
   in quickstream run asynchronously it will not run the same in each
   run.  You might have an adjustable variable in the program that lets
   is run for a long time if the variable is large, and then the
   checked-in code must make it run quickly, like 0.01 seconds on a fast
   computer.

 - Tests that are added must be "tested" with DEBUG and regular
   quickstream builds.  In both cases the test must do the "right" thing.
   If the test requires a DEBUG build than the test must automatically
   skip itself for regular builds.  Etc ...

##More

The test programs in this directory take longer to compile than the
package library because they test some of the inner guts of the library
and have to statically link to the library objects in order to get access
to some of the internal parts of libquickstream.  You can save time by
running the parallelized make like for example: "make -j 12".

If a particular test program fails just once in a while either make a
changed version that fails more often, or run it in a bash while loop
that bails when it fails.

These tests are by far the most important development part of quickstream.
quickstream would not exist without these tests.


# Bash script to run ./run_tests many times.

Note: since there are many test programs that do not run in a
deterministic fashion, some tests can fail just some of the time.  I have
seen bugs from tests that fail only 1 time in about 100 runs of a test.
You may be able to find bugs in tests more easily by changing the test to
make it take a longer time to run.  Of course, please do not make and
check-in tests that run with ./run_tests and take longer that about a
second to run.

Try this:

~~~
$ x=0; start="$(date)"; while ./run_tests ;\
do let x=$x+1; echo -e "\n\n\n\
$x   \n\n\n"; sleep 0.8 ; done; echo -e "\nStart: $start\n";\
echo -n "Failed: "; date
~~~
