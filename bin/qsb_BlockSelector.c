#ifndef _GNU_SOURCE
#  define _GNU_SOURCE
#endif

#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>

#include <gtk/gtk.h>

#include "../lib/debug.h"
#include "qsb_treeView.h"

// One of the crappy things about modules is how do different programs
// find said modules.  It starts at the OS (operating system) level with
// the linker/loader system, and there are end user programs that need to
// find a series of configuration files.  It's at the core of what makes
// operating systems suck.  What is needed is an OS built with
// installation management system that is built into the file system;
// so something like APT is built into the file system.



// Get the directory based on the path of this running program.
//
// This function cannot fail.  The program is of no use if this fails.
//
// The returned value must be free()ed.
//
// The program that we are running in in /proc/self/exe
//
// Example: exec path /home/joe/git/quickstream/bin/quickstreamProgram
//
// ==> module path returned:
//    /home/joe/git/quickstream/lib/quickstream/blocks
//

// We'll be adding this to /home/joe/git/quickstream/

#define ADDSTR  ("lib/quickstream/blocks")

static
char *GetModuleDirectory(void) {

    size_t addlen = strlen(ADDSTR);
    ssize_t bufLen = 128 + addlen;

    char *buf = malloc(bufLen);
    ASSERT(buf, "malloc(%zu) failed", bufLen);
    // We need to append to the string, so we do not let readlink write the
    // whole string that we allocated.
    ssize_t rl = readlink("/proc/self/exe", buf, bufLen);
    errno = 0;
    ASSERT(rl > 0, "readlink(\"/proc/self/exe\",,) failed");
    while(rl >= bufLen - addlen) {
        // I think the OS should guarantee this:
        DASSERT(rl == bufLen + addlen);
        // If it gets this large we decide to give up.
        ASSERT(bufLen < 1024*1024);
        buf = realloc(buf, bufLen += 128);
        ASSERT(buf, "realloc(,%zu) failed", bufLen);
        rl = readlink("/proc/self/exe", buf, bufLen);
        ASSERT(rl > 0, "readlink(\"/proc/self/exe\",,) failed");
    }
    buf[rl] = '\0';
    // move from end to 2nd to last '/'
    char *str = buf + (rl-1);
#define DIRCHAR  ('/')
    while(str != buf && *str != DIRCHAR) --str;
    // at /home/joe/git/quickstream/bin/
    if(str != buf) --str;
    // at /home/joe/git/quickstream/bin
    while(str != buf && *str != DIRCHAR) --str;
    // at /home/joe/git/quickstream/
    // This can't fail.
    ASSERT(*str == DIRCHAR);
    ++str;
    *str = '\0'; 

    strcpy(str, ADDSTR);

    return buf;
}



static
bool CheckFile(int dirfd, const char *path) {

    size_t len = strlen(path);
    if(len <= 3) return false;
    bool ret = false;

    // All these calls must succeed.  We do not care so much why, we just
    // know that if we can't do all this, this file is not usable for as a
    // quickstream plugin, so it, by definition, is not a valid plugin.
    

    if(strcmp(&path[len-3], ".so") != 0)
        return false;

    char *pwd = get_current_dir_name();
    ASSERT(pwd, "get_current_dir_name() failed");


    if(fchdir(dirfd) != 0) {
        // fail
        free(pwd);
        return false;
    }

    char *filename = malloc(strlen("./") + strlen(path) + 1);
    ASSERT(filename, "malloc() failed");
    sprintf(filename, "./%s", path);
    void *dlhandle = dlopen(filename, RTLD_LAZY);
    free(filename);
    if(!dlhandle)
        goto cleanup;

    void *declare = dlsym(dlhandle, "declare");
    if(declare)
        // success.  This may be a usable plugin.
        ret = true;

cleanup:

    ASSERT(chdir(pwd) == 0);
    free(pwd);
    if(dlhandle)
        dlclose(dlhandle);

    return ret;
}


static
char *GetName(int dirfd, const char *path) {
    DASSERT(path);
    char *name = strdup(path);
    ASSERT(name, "strdup() failed");

#if 0 // TODO: THIS OR NOT
    // Strip off the ".so" that is on the end.
    size_t len = strlen(path);
    DASSERT(len >= 3);
    DASSERT(path[len-3] == '.');
    DASSERT(path[len-2] == 's');
    DASSERT(path[len-1] == 'o');
    name[len-3] = '\0';
#endif

    return name;
}


static
void
rowActivated_CB(GtkTreeView       *tree_view,
               GtkTreePath       *path,
               GtkTreeViewColumn *column,
               gpointer           user_data) {

    ERROR();
}



void AddBlockSelector(GtkWidget *tree) {

    ASSERT(tree);
    treeViewCreate(tree);
    char *moduleDir = GetModuleDirectory();
    // Make the top path label
    const char *pre = "core (";
    const char *post = ")";
    size_t len = strlen(pre) + strlen(moduleDir) + strlen(post) + 1;
    char *pathLabel = malloc(len);
    ASSERT(pathLabel, "malloc(%zu) failed", len);
    strcpy(pathLabel, pre);
    strcpy(pathLabel + strlen(pre), moduleDir);
    strcpy(pathLabel + strlen(pre) + strlen(moduleDir), post);
    treeViewAdd(tree, moduleDir, pathLabel, CheckFile, GetName);
    treeViewShow(tree, "Choose Block to Create");

    //"activate-on-single-click"
    
    g_signal_connect(G_OBJECT(tree),"row-activated",
            G_CALLBACK(rowActivated_CB), 0);

    free(pathLabel);
    free(moduleDir);
}
