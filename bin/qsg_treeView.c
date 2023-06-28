// The test program ./treeView_directoryTest show that this does not leak
// file descriptors.  This code opens and closes lots of files.
//
// To test for file descriptor leaks run ./treeView_directoryTest and then
// look in /proc/PID/fd/ (there PID is the PID of that program) with some
// command like: ls -l /proc/PID/fd/
//
// All the file descriptors should not be related to files that the
// files and directories that this program opened.

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <gtk/gtk.h>

#include "../lib/debug.h"
#include "../lib/Dictionary.h"

#include "./qsg_treeView.h"
#include "./quickstreamGUI.h"

#define DIRCHAR  ('/')



GtkWidget *treeViewCreate(void) {

    GtkWidget *treeView = gtk_tree_view_new();
    DASSERT(treeView);

    // Set tree view property "has-tooltip"
    gtk_widget_set_has_tooltip(treeView, TRUE);
    gtk_tree_view_set_tooltip_column(GTK_TREE_VIEW(treeView),
            1/*We use this column for tooltip*/);

    // We can add more columns here:
    GtkTreeStore *store =
        gtk_tree_store_new(2,/*number of columns*/
            G_TYPE_STRING/*0 path gets rendered*/,
            G_TYPE_STRING/*1 tooltip description*/);

    gtk_tree_view_set_model(GTK_TREE_VIEW(treeView),
            GTK_TREE_MODEL(store));

    return treeView;
}


// This returns true if a file is found that passes the CheckFile() call
// in a recursive call of this function.
//
// if(path) is set we just check if the path passes the CheckFile() test.
// else if(!path) we check all the directories there in dirfd.
//
// This will close the dirfd.
//
static
bool CheckDir(int dirfd,
        bool (*CheckFile)(int dirfd, const char *path),
        uint32_t recurseMax) {

    int dirFd = -1;
    DIR *dir = 0;
    bool ret = false;

    if(--recurseMax == 0)
        goto cleanup;

    // dirFd will be like dirfd, but separate; so that we may use it
    // to openat() below.
    dirFd = openat(dirfd, ".", 0);
    if(dirFd < 0)
        // Houston, we have a problem.
        goto cleanup;

    // This will trash dirfd, which is why we needed the dirFd opened
    // above.
    dir = fdopendir(dirfd);
    if(!dir)
        // This may not be a directory
        goto cleanup;


    struct dirent *ent = readdir(dir);

    while(ent) {

        switch(ent->d_type) {
            case DT_REG:
                // sanity check.
                DASSERT(ent->d_name && ent->d_name[0]);
                if(CheckFile(dirFd, ent->d_name)) {
                    // Once we find one we search no more.
                    ret = true;
                    goto cleanup;
                }
                break;
            case DT_DIR:
                 if(strcmp(ent->d_name, ".") == 0 ||
                        strcmp(ent->d_name, "..") == 0) break;
            {
                int fd = openat(dirFd, ent->d_name, 0);
                if(fd >= 0) {
                    // Directory dive.
                    if(CheckDir(fd, CheckFile, recurseMax)) {
                        // success.
                        ret = true;
                        // fd is now closed.
                        goto cleanup;
                    }
                }
                // else keep looking for success.
            }
                break;
        }
        ent = readdir(dir);
    }

cleanup:

    if(dir)
        // This will close dirfd.
        closedir(dir);
    else if(dirfd >= 0)
        close(dirfd);

    if(dirFd >= 0)
        close(dirFd);

    return ret;
}


// This will close dirfd.
//
static
void AddView(GtkTreeStore *store, const char *path,  const char *name,
        int dirfd, GtkTreeIter *parentIt,
        bool (*CheckFile)(int dirfd, const char *path),
        char * (*GetName)(int dirfd, const char *path),
        uint32_t recurseMax) {

    if(!name) name = path;

    DIR *dir = 0;
    int dirFd = -1;

    if(--recurseMax == 0)
        goto cleanup;

    // dirFd will be like dirfd, but separate; so that we may use it
    // to openat() below.
    dirFd = openat(dirfd, ".", 0);
    if(dirFd < 0)
        // Houston, we have a problem.
        // I guess it was not that important.
        goto cleanup;

    dir = fdopendir(dirfd);
    if(!dir)
        goto cleanup;
    // Now dirfd is trashed by dir, and we use dir to cleanup resources,
    // not dirfd.

    struct dirent *ent = readdir(dir);

    while(ent) {

        switch(ent->d_type) {
            case DT_REG:
                if(CheckFile(dirfd, ent->d_name)) {
                    GtkTreeIter iter;
                    char *name = GetName(dirfd, ent->d_name);
                    const size_t len = 256;
                    char tooltip[len];
                    snprintf(tooltip, len,
                            "double click to load: %s", name);
                    gtk_tree_store_append(store, &iter, parentIt);
                    gtk_tree_store_set(store, &iter,
                            0, name,
                            1, tooltip,
                            -1);
                    free(name);
                }
                break;
            case DT_DIR:
                if(strcmp(ent->d_name, ".") == 0 ||
                        strcmp(ent->d_name, "..") == 0) break;
            {
                int fd = openat(dirFd, ent->d_name, 0);
                if(fd >= 0) {
                    if(CheckDir(fd, CheckFile, recurseMax)) {

                        GtkTreeIter iter;
                        const size_t len = 280;
                        char tooltip[len];
                        snprintf(tooltip, len, "sub directory: %s",
                                ent->d_name);
                        gtk_tree_store_append(store, &iter, parentIt);
                        gtk_tree_store_set(store, &iter,
                                0, ent->d_name,
                                1, tooltip,
                                -1);
                        // CheckDir() closed it.  We open it again.
                        fd = openat(dirFd, ent->d_name, 0);
                        if(fd >= 0)
                            // Dive.  Dive.
                            AddView(store, ent->d_name, 0, fd, &iter,
                                    CheckFile, GetName, recurseMax);
                    }
                }
            }
                break;
        }
        ent = readdir(dir);
    }

cleanup:

    if(dir)
        closedir(dir);
    else if(dirfd >= 0)
        close(dirfd);

    if(dirFd >= 0)
        close(dirFd);
}


// Built-in block modules are not installed in directory trees in the file
// system.  Built-in block modules are in a (dictionary) list.
// libquickstream.so has built-in block modules compiled into itself.
// There was a directory tree structure associated with them, but it is
// not installed, because all the symbols that make up the interfaces to
// the built-in become compiled into libquickstream.so.
//
// Built-in modules must be small with no added library dependencies, as
// to not bloat libquickstream.so.  The whole point of built-in block
// modules is to keep them very small, simple, common, and essential core
// block modules in libquickstream.so.  It is very questionable as to
// whither or not built-in block modules should even exist.  Some kind of
// bench mark is needed to see if there is any benefit to having them, but
// to do that we first need to make them exist.  And so here we are ...
//
// Return true on success, and false on failure.
//
bool treeViewAdd_usingStringArray(
        GtkWidget *tree, GtkTreeIter *topIter,
        const char * const builtInBlocks[]) {

    GtkTreeStore *store = GTK_TREE_STORE(
            gtk_tree_view_get_model(GTK_TREE_VIEW(tree)));
    ASSERT(store);

    // Remember the strings in builtInBlocks are not in directories and
    // files, they come from symbols that are compiled into
    // libquickstream.so, and that's why we call them built-in block
    // modules.
    //
    // Don't worry we will sort the tree view later.
    //
    //DSPEW("Built-in block modules:");
    for(const char * const *str = builtInBlocks; *str; ++str) {
        //fprintf(stderr, "          %s\n", *str);
        // It cannot be an empty string.
        DASSERT((*str)[0]);
        const size_t len = 512;
        char name[len];
        name[0] = '\0';
#ifdef DEBUG
        // Check that no built-in block name is too long.
        ASSERT(strlen(*str) < len, "Increase len in this code!!");
#endif
        // So, *str is like "foo/bar/blockName" with no file suffix.
        strcpy(name, *str);

        //DSPEW("name=%s", name);

        // We change the stack memory in char name[len] as we go through
        // it.
        //
        // We start with the parent iterator being the one that was passed
        // into this function.
        GtkTreeIter *parentIt = topIter;
        // We need this memory, parent, if there is more than one
        // sub-directory to get to the block.
        GtkTreeIter parent;

        // n is the sub name iterator like: "foo" "bar" "blockName" in the
        // above example.
        for(char *n = name; *n;) {
            // i is the character iterator that goes through n.
            char *i = n;
            for(; *i && *i != DIRCHAR; ++i);
            if(*i == DIRCHAR) {
                *i = '\0';
                ++i;
            }
            // now n is a sub name and is a terminated string without
            // a DIRCHAR separator in it.  For example it's: "foo", "bar",
            // or "blockName".
            //DSPEW("n=%s", n);

            const size_t len = 256;
            char tooltip[len];
            GtkTreeIter child;

            if(!(*i)) {

#ifdef DEBUG
                // We will make a new child node and so there must not be
                // a node with this name already.
                bool foundIt = false;

                for(gint j = gtk_tree_model_iter_n_children(
                            GTK_TREE_MODEL(store), parentIt) - 1;
                        j != -1; --j) {
                    // This parent has children so we must search for this
                    // name, n.
                    gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(store),
                            &child, parentIt, j);
                    GValue val = G_VALUE_INIT;
                    gtk_tree_model_get_value(GTK_TREE_MODEL(store),
                            &child, 0/*column*/, &val);
                    ASSERT(G_VALUE_HOLDS_STRING(&val));
                    const char *s = g_value_get_string(&val);
                    //DSPEW("val=\"%s\"", s);
                    if(strcmp(s, n) == 0) {
                        foundIt = true;
                        g_value_unset(&val);
                        break;
                    }
                    g_value_unset(&val);
                }
                ASSERT(!foundIt,
                        "There is a tree view node with path %s already",
                        *str);
#endif

                // Now we make the leaf node for a block module.
                snprintf(tooltip, len, "double click to load built in: %s", n);
                gtk_tree_store_append(store, &child, parentIt);
                gtk_tree_store_set(store, &child, 0, n, 1, tooltip, -1);
                // We are done advancing the GtkTreeIter parent iterator.
            } else {

                // This is a directory-like node.

                // We need to make a parent node with (string) n if it
                // does not exist already, but if it exists already we
                // just need to find that node and make parentIt be an
                // iterator for it.
                bool foundIt = false;

                for(gint j = gtk_tree_model_iter_n_children(
                            GTK_TREE_MODEL(store), parentIt) - 1;
                        j != -1; --j) {
                    // This parent has children so we must search for this
                    // name, n.
                    gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(store),
                            &child, parentIt, j);
                    GValue val = G_VALUE_INIT;
                    gtk_tree_model_get_value(GTK_TREE_MODEL(store),
                            &child, 0/*column*/, &val);
                    ASSERT(G_VALUE_HOLDS_STRING(&val));
                    const char *s = g_value_get_string(&val);
                    //DSPEW("val=\"%s\"", s);
                    if(strcmp(s, n) == 0) {
                        // This node child must have children otherwise
                        // it's a leaf node that has the path to a block
                        // that should not have children.
                        ASSERT(gtk_tree_model_iter_n_children(
                            GTK_TREE_MODEL(store), &child) > 0,
                                "Found tree view node at \"%s\" "
                                "in the way of "
                                "built-in module \"%s\"", s, *str);
                        foundIt = true;
                        g_value_unset(&val);
                        break;
                    }
                    g_value_unset(&val);
                }

                if(!foundIt) {
                    // We do not have an entry at this level with name, n,
                    // yet; so we make it now.
                    snprintf(tooltip, len, "sub directory: %s", n);
                    gtk_tree_store_append(store, &child, parentIt);
                    gtk_tree_store_set(store, &child, 0, n,
                            1, tooltip, -1);
                }

                // Advance GtkTreeIter parent iterator thingy to the child
                // for the next loop iteration.  The child becomes the
                // parent, like with humans when the parent gets to
                // old; okay not exactly.
                //
                memcpy(&parent, &child, sizeof(child));
                parentIt = &parent;
            }
            // Go to the next sub name or be done if i is 0.
            n = i;
        }


    }

    return true; // true => success
}


// Returns true if we found and made some entries.
//
// path is a path to a directory that we can open.
//
// topIter can be 0, or will be used as the memory for the "top"
// iterator at this level.
//
bool treeViewAdd(GtkWidget *tree,
        const char *path,  const char *name, GtkTreeIter *topIter,
        bool (*CheckFile)(int dirfd, const char *path),
        char *(*GetName)(int dirfd, const char *path)) {

    int fd = open(path, 0);
    if(fd <= 0) {
        errno = 0;
        return false;
    }
    GtkTreeStore *store = GTK_TREE_STORE(
            gtk_tree_view_get_model(GTK_TREE_VIEW(tree)));
    ASSERT(store);


    const uint32_t recurseMax = 2000;

    bool ret = CheckDir(fd, CheckFile, recurseMax);
    if(ret) {

        // CheckDir() close the fd so we need to open it again.
        fd = open(path, 0);
        if(fd <= 0) {
            errno = 0;
            return false;
        }

        // Stack memory iterator.
        GtkTreeIter sIter;
        if(!topIter)
            // Use stack memory for the iterator by default.
            topIter = &sIter;
        // But if the user pasted in a pointer to memory we will use it,
        // and then the user can use it to add entries at the point in the
        // tree view.

        gtk_tree_store_append(store, topIter, 0);
        const size_t len = 256;
        char tooltip[len];
        snprintf(tooltip, len, "directory: %s", path);
        gtk_tree_store_set(store, topIter,
                0, name,
                1, tooltip,
                -1);

        AddView(store, path, name, fd, topIter, CheckFile, GetName,
                recurseMax);
    }

    return ret;
}


void treeViewShow(GtkWidget *tree, const char *header) {

    // Sort it first:
    GtkTreeStore *store = GTK_TREE_STORE(
            gtk_tree_view_get_model(GTK_TREE_VIEW(tree)));
    ASSERT(store);
    // This seems to sort the block module files into alphabetical order.
    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(store),
                        0, GTK_SORT_ASCENDING);

    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();

    GtkTreeViewColumn *column =
        gtk_tree_view_column_new_with_attributes(header,
                                        renderer,
                                        "text", 0,
                                        NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

    gtk_tree_view_expand_all(GTK_TREE_VIEW(tree));

    gtk_widget_show(tree);
}
