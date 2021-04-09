// This file is basically a singleton class object for the GTK tree view
// that we use to select the block file name.  In C that kind of shit is
// dumb-ass simple.  No fuck'n around.

#ifndef _GNU_SOURCE
// For get_current_dir_name()
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


// One of the crappy things about modules is address how do different
// programs find said modules.  It starts at the OS (operating system)
// level with the linker/loader system, and there are end user programs
// that need to find a series of configuration files.  It's at the core of
// what makes operating systems suck.  What is needed is an OS built with
// installation management system that is built into the file system; so
// something like (Debian) APT is built into the file system.  Of course
// it'd look more like NixOS Nix package manager than Debian APT.  The Nix
// package manager shits all over the file system, making it seem obvious
// that a kernel system hack is what is needed.  Such a file system would
// be a major part of the ultimate operating system.  Maybe start with all
// the cool shit that is in the ZFS file system.  Now you are completely
// lost, but I don't give a shit.  I'm quite insane, or is it the rest of
// the world just bunch of dumb mother fuckers.


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
GtkEntry *selectedBlockEntry = 0;


const char *GetSelectedBlockFile(void) {
    DASSERT(selectedBlockEntry);

    static char *ret = 0;

    if(ret) free(ret);

    // This string points to internally allocated storage in the widget
    // and must not be freed, modified or stored.
    const char *blockFile = gtk_entry_get_text(selectedBlockEntry);

    if(!blockFile || !blockFile[0])
        ret = 0;
    else
        ret = strdup(blockFile);

    // This is why we had to copy the text.
    gtk_entry_set_text(selectedBlockEntry, "");

    return ret;
}


static
void
rowActivated_CB(GtkTreeView       *tree,
               GtkTreePath       *tpath,
               GtkTreeViewColumn *column,
               gpointer           user_data) {

    GtkTreeModel *store = gtk_tree_view_get_model(GTK_TREE_VIEW(tree));
    GtkTreeIter iter;
    gtk_tree_model_get_iter(store, &iter, tpath);
    gboolean hasChildren = gtk_tree_model_iter_has_child(store, &iter);

    if(hasChildren)
        // We just get selections that are children.
        return;

    char *selectedBlockFile = 0;


    gboolean ret = TRUE;

    while(ret) {

        gchar *name;
        gtk_tree_model_get(store, &iter, 0/*column*/, &name, -1);
        GtkTreeIter parentIter;
        ret = gtk_tree_model_iter_parent(store, &parentIter, &iter);
        iter = parentIter;

        if(ret) {
            size_t l0 = 0;
            char *oldItem = 0;
            if(selectedBlockFile) {
                l0 = strlen(selectedBlockFile);
                oldItem = selectedBlockFile;
            }
            size_t l1 = l0 + strlen(name) + 2;
            selectedBlockFile = malloc(l1);
            ASSERT(selectedBlockFile, "malloc(%zu) failed", l1);
            if(oldItem)
                sprintf(selectedBlockFile, "%s/%s", name, oldItem);
            else
                strcpy(selectedBlockFile, name);

            if(oldItem)
                free(oldItem);
        }
        g_free(name);
    }

    DSPEW("selectedBlockFile=%s", selectedBlockFile);

    gtk_entry_set_text(selectedBlockEntry, selectedBlockFile);

    free(selectedBlockFile);
}



void AddBlockSelector(GtkWidget *tree, GtkEntry *entry) {

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

    // "activate-on-single-click"
    g_signal_connect(G_OBJECT(tree),"row-activated",
            G_CALLBACK(rowActivated_CB), 0);

    free(pathLabel);
    free(moduleDir);

    selectedBlockEntry = entry;
}


// We let the gtk widget API handle cleanup of the GTK Tree View, but
// there is some cleanup needed.
//
void CleanupBlockSelector(void) {

    DSPEW();
}
