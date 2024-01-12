#include <stdio.h>
#include <stdint.h>
#include <gtk/gtk.h>

#include "../include/quickstream.h"

#include "../lib/debug.h"
#include "../lib/mprintf.h"
#include "../lib/Dictionary.h"


#include "quickstreamGUI.h"
#include "qsg_treeView.h"




GtkTreePath *GetTreePath(GtkTreeView *treeView) {

    GtkTreeSelection *sel = gtk_tree_view_get_selection(treeView);
    DASSERT(sel);
    GtkTreeIter iter;
    GtkTreeModel *model;

    if(!gtk_tree_selection_get_selected(sel, &model, &iter))
        // Nothing is selected in the blocks list tree view widget.
        return 0;

    // I think at this point something is selected so:
    GtkTreePath *tpath = gtk_tree_model_get_path(model, &iter);
    // So really, something must be selected.
    DASSERT(tpath);

    return tpath;
}

// Returns name to add the file, or 0 to not.
//
// The returned value must be free()d.
static
char *GetName(int dirfd, const char *path) {

    DASSERT(path);
    char *name = strdup(path);
    ASSERT(name, "strdup() failed");
    return name;
}


// Returns true to add the file, or false to not.
//
// The dirfd should not be closed by this function.
static
bool CheckFile(int dirfd, const char *path) {

    size_t len = strlen(path);
    if(len <= 3) return false;

    if(strcmp(&path[len-3], ".so") == 0 &&
            // We consider files beginning with '_' as hidden files
            // that the modules need but do not show to the user.
            *path != '_') {
        //DSPEW("%s", path);
        return true;
    }
    return false;
}


// Returned pointer must be free()ed.
//
// Get the full path to a block source file, if we can find it.
//
char *GetBlockSourcePath(GtkTreeView *treeView) {

    DASSERT(treeView);

    GtkTreePath *tpath = GetTreePath(treeView);
    if(!tpath)
        // We need a thing selected and tpath will refer to what is
        // selected.
        // This should not have been called if nothing was selected;
        // but okay, we'll return 0.
        return 0;

    GtkTreeIter itA;
    GtkTreeIter itB;
    GtkTreeIter *parent = &itB;
    GtkTreeIter *it = &itA;
    GtkTreeModel *model = gtk_tree_view_get_model(treeView);
    ASSERT(model);
    gtk_tree_model_get_iter(model, it, tpath);


    if(gtk_tree_model_iter_children(model, parent/*dummy child*/, it))
        // WTF: This iterator, it, has children; so the selected node
        // cannot be a leaf node (block).
        return 0;

    int index = 0;

    char *name;
    gtk_tree_model_get(model, it, index, &name, -1);
    ASSERT(name);
    ASSERT(name[0]);
    // This name should be the basename of the block like:
    // "foo.so" (DSO block) or "foo" (built-in block).
    char *path = strdup(name);
    ASSERT(path, "strdup() failed");
    // Note: We do not assume that g_free() is the same as free().
    g_free(name);

    while(gtk_tree_model_iter_parent(model, parent, it)) {

        // Go up the tree toward a root node.

        // Switch the child, "it", to point to the parent memory and the
        // parent to point to the memory that we will overwrite in the
        // next gtk_tree_model_iter_parent() call setting parent to the
        // next parent.
        if(it == &itA) {
            DASSERT(parent == &itB);
            it = &itB;
            parent = &itA;
        } else {
            DASSERT(it == &itB);
            DASSERT(parent == &itA);
            it = &itA;
            parent = &itB;
        }

        // "it" is the next parent.
        if(gtk_tree_model_iter_parent(model, parent, it))
            index = 0;
        else
            // if we have no parent for this "it" node than
            // this is a top level node and
            // we get the index [2] in the store entry
            // which is the rest of the path root.
            // This path could be full or relative depending on
            // if QS_BLOCK_PATH is used and it is full or relative.
            index = 2;

        gtk_tree_model_get(model, it, index, &name, -1);
        ASSERT(name);
        ASSERT(name[0]);
        char *dir = path;

        // TODO: Linux specific code, directory separator "/":

        // Concatenate and allocate a longer path.
        path = mprintf("%s/%s", name, dir); // mprintf() will call
                                            // ASSERT()
        // Free the old shorter path.
        free(dir);
        g_free(name);
    }

    // TODO: Linux specific code ".so" and not ".dll".

    size_t len = strlen(path);

    if(len > 3 && strcmp(path + len - 3, ".so") == 0) {
        // This block is a DSO and not a built-in.
        //
        // Change the ".so" suffix to ".c".
        path[len - 2] = 'c';
        path[len - 1] = '\0';
    } else {
        // This block is a built-in block.
        //
        // Add a ".c" suffix to path.
        char *npath = mprintf("%s.c", path);
        free(path);
        path = npath;
    }

    char *rpath = realpath(path, 0);

#ifdef DEBUG
    if(!rpath)
        DSPEW("failed to get realpath() for \"%s\"", path);
#endif

    free(path);

    // Now we have something like path = "/root/a/b/c/d/foo.c" or if there
    // is no file path == 0.
    //
    // If set, path must be free()ed by the caller.
    return rpath;
}


// Get the block path from the tree view widget that was "clicked" or
// whatever widget activation signal.
//
// Note: this function recurses; calls itself.
//
// So it uses the function call stack with each function call having a
// substring (like "dir2/") of a string that it allocates and sets the
// content of (like "root/dir1/dir2/file.so").
//
// The returned value must be free()ed, or not if it returned 0.
//
static
char *_GetBlockLoadPath(GtkTreeView *treeView, GtkTreePath *tpath,
        GtkTreeModel *model,
        GtkTreeIter *child, size_t *len, size_t *addLen) {

    DASSERT(treeView);

    if(!tpath) {
        // We need a thing selected and tpath will refer to what is
        // selected.
        tpath = GetTreePath(treeView);
        if(!tpath)
            // This should not have been called if nothing was selected;
            // but okay, we'll return 0.
            return 0;
    }

    GtkTreeIter parent;

    if(!len) {
        // If this is a leaf node we will start with the
        // path being the name of the leaf and recurse
        // to prepend the directory names to it.
        ASSERT(!model);
        ASSERT(!child);
        ASSERT(addLen == 0);
        model = gtk_tree_view_get_model(treeView);
        ASSERT(model);
        size_t len = 0;
        char *name = 0;
        GtkTreeIter it;
        gtk_tree_model_get_iter(model, &it, tpath);

        if(gtk_tree_model_iter_children(model, &parent/*dummy child*/, &it))
            // This iterator, it, has children, and so is not
            // a leaf node.
            return 0;

        if(!gtk_tree_model_iter_parent(model, &parent, &it))
            // WTF: This iterator, it, does not have a parent.
            // We did not make the tree like that.
            return 0;

        gtk_tree_model_get(model, &it, 0, &name, -1);
        ASSERT(name);

        len = strlen(name);

        char *path = _GetBlockLoadPath(treeView, tpath, model, &it, &len, 0);
        sprintf(path + strlen(path), "%s", name);

        g_free(name);
        return path;
    }

    ASSERT(child);

    if(gtk_tree_model_iter_parent(model, &parent, child)) {
        char *name = 0;
        gtk_tree_model_get(model, &parent, 0, &name, -1);
        ASSERT(name);
        size_t addLen = strlen(name) + 1;
        *len += addLen;
        char *path = _GetBlockLoadPath(treeView, tpath, model,
                &parent, len, &addLen);
        if(!addLen) {
            // We skip adding the last one.
            g_free(name);
            return path;
        }
        // TODO: Linux specific '/' directory separator.
        sprintf(path + strlen(path), "%s/", name);
        g_free(name);
        return path;
    }

    // This is the top of the tree.
    if(!addLen) return 0;

    ASSERT(*len > *addLen);

    *len -= *addLen;
    *addLen = 0; // to mark skip this node.

    // We need the memory to be zeroed, so that we see the string length
    // as we fill it.  calloc() zeros the memory it allocates.
    char *path = calloc(1, *len + 1);
    ASSERT(path, "calloc(1,%zu) failed", *len+1);
    return path;
}


// Used to get a block path used by qsGraph_createBlock(), to load a block
// into the flow graph, by the search rules defined by
// libquickstream.so.
//
// Note: not the same as GetBlockSourcePath() which gets a path to the
// blocks main source C or C++ file, path.c or path.cpp.
char *GetBlockLoadPath(GtkTreeView *treeView, GtkTreePath *tpath) {

    return _GetBlockLoadPath(treeView, tpath, 0, 0, 0, 0);
}


static inline
char *GetDirPath(GtkTreeView *treeView, GtkTreePath *tpath) {

    DASSERT(treeView);
    DASSERT(tpath);

    GtkTreeIter itA;
    GtkTreeIter itB;
    GtkTreeIter *parent = &itB;
    GtkTreeIter *it = &itA;
    GtkTreeModel *model = gtk_tree_view_get_model(treeView);
    ASSERT(model);
    gtk_tree_model_get_iter(model, it, tpath);

    if(!gtk_tree_model_iter_children(model, parent, it)) {
        // This iterator does not have children, and so is not a
        // directory node that should exist.  We should not have this
        // case.  We are in a bad way, but we do not need to quit
        // running the program ... ???
        //
        // This is do to a coding error in quickstreamGUI...
        ASSERT(0, "GTK3 tree directory does not have children");
        return 0;
    }

    int index = 0;

    if(gtk_tree_model_iter_parent(model, parent, it))
        // We have a parent node.
        index = 0;
    else
        // We do not have a parent node.
        //
        // For top most nodes the index 2 entry is the path we seek.
        index = 2;

    char *name;
    gtk_tree_model_get(model, it, index, &name, -1);
    ASSERT(name);
    ASSERT(name[0]);
    char *path = strdup(name);
    ASSERT(path, "strdup() failed");
    // Note: We do not assume that g_free() is the same as free().
    g_free(name);


    while(gtk_tree_model_iter_parent(model, parent, it)) {

        // Go up the tree toward a root node.

        // Switch the child, "it", to point to the parent memory and the
        // parent to point to the memory that we will overwrite in the
        // next gtk_tree_model_iter_parent() call setting parent to the
        // next parent.
        if(it == &itA) {
            DASSERT(parent == &itB);
            it = &itB;
            parent = &itA;
        } else {
            DASSERT(it == &itB);
            DASSERT(parent == &itA);
            it = &itA;
            parent = &itB;
        }

        // "it" is the next parent.
        if(gtk_tree_model_iter_parent(model, parent, it))
            index = 0;
        else
            // if we have no parent for this "it" node than
            // this is a top level node and
            // we get the index [2] in the store entry
            // which is the rest of the path root.
            // This path could be full or relative depending on
            // if QS_BLOCK_PATH is used and it is full or relative.
            index = 2;

        gtk_tree_model_get(model, it, index, &name, -1);
        ASSERT(name);
        ASSERT(name[0]);
        char *dir = path;

        // TODO: Linux specific code, directory separator "/":

        // Concatenate and allocate a longer path.
        path = mprintf("%s/%s", name, dir); // mprintf() will call
                                            // ASSERT()
        // Free the old shorter path.
        free(dir);
        g_free(name);
    }

    // Now we have something like path = "root/a/b/c/d"
    //
    // path must be free()ed by the caller.
    return path;
}


// Can add a block to the current work area from the current tree view
// that is selected/clicked.
//
static void TreeView_Select_cb(GtkTreeView *treeView,
        GtkTreePath *tpath, GtkTreeViewColumn *column,
        struct Window *w) {

    DASSERT(w);
    DASSERT(w->layout);

    char *path = GetBlockLoadPath(treeView, tpath);
    if(!path)
        // A upper lower branch of the tree was acted on, and
        // not a leaf.  No path memory to free.
        return;

    DSPEW("path=\"%s\"", path);

    // Try to open a block with path.
    //
    // AddBlock will own the path memory.
    //AddBlockFromPath(window->currentGraph, path, -1.0, -1.0);
    
    // orientation Position of  In, Out, Set, Get
    //  2 bits each x 4 = 8 bits.
    //  In is the first 2 bits.
    //  Out is the next 2 bits.
    //  and so on...
    //uint8_t orientation;

    CreateBlock(w->layout, path, 0/*block*/,
        -1.0/*x*/, -1.0/*y*/, 0/*orientation*/,
        0 /*0 unselectAllButThis,
            1 add this block to selected
            2 no selection change */);


    free(path);
}


static gboolean
TreeViewButtonRelease_cb(GtkTreeView *treeView, GdkEventButton *e,
               void *userData) {

    if(e->type != GDK_BUTTON_RELEASE || e->button != 3 /*not left mouse*/)
        return FALSE;

    // Now we have a left mouse release event.

    // TODO: We wanted to popup the popup menu on a button press, but we
    // found many buggy things in GTK3 that made that problematic.  I
    // burned a whole day trying many versions of work-a-rounds.
    //
    // You see: if we do it with a button press we may not have all the
    // information we need at the time of this event.  The next event
    // handler in the queue may do the selection and then we can get the
    // tpath from the "current" selection; so we may have a popup without
    // the correct block selected by using button press.
    //
    // So we decided that showing pop up widget on a button release event
    // is close enough.  It's just a little slower for the user; not as
    // snappy.

    DASSERT(treeView);

    char *path = GetBlockLoadPath(treeView, 0);

    if(path) {
        DASSERT(path[0]);
        DSPEW("path=\"%s\"", path);
        // The memory for path is owned by the ShowTreeViewPopupMenu()
        // thingy.
        ShowTreeViewPopupMenu(treeView, path, 0);
        return FALSE; // FALSE => go to the next handler.
    }

    // At this point we do not have a block path selected in the tree
    // view.
    //
    // TODO: some of this could be mash together with the
    // GetBlockLoadPath() above.
    //
    GtkTreePath *tpath = GetTreePath(treeView);
    if(!tpath)
        // This should not have been called if nothing was selected;
        // but okay, we'll return 0.
        return FALSE; // FALSE => go to the next handler.

    // We have not selected a leaf node in the tree.  It must be a
    // directory in the file system.

    path = GetDirPath(treeView, tpath);

    // We have not selected a leaf node in the tree.  It must be a
    // directory.
    //
    // ShowTreeViewPopupMenu() will free path if it is set.
    //
    ShowTreeViewPopupMenu(treeView, 0, path);

    return FALSE; // FALSE => go to the next handler.
}


void
CreateTreeView(GtkContainer *sw, struct Window *w) {

    GtkWidget *tree = treeViewCreate();

    g_signal_connect(tree, "button-release-event",
                    G_CALLBACK(TreeViewButtonRelease_cb), 0);

    // TODO: We may need to add code to check for overlapping
    // block names in the tree view widget; and then do what, I'm
    // not sure, but do something like fail or skip them.

    DSPEW("pluginRoot= %s", qsBlockDir);
    GtkTreeIter iter;
    // If we do not get Core and built-in block modules into the
    // tree view widget we are fucked (hence ASSERT()).
    //
    // qsBlockDir is a full path.
    //
    // First core modules:
    ASSERT(treeViewAdd(tree, qsBlockDir, "Core", &iter,
            CheckFile, GetName));
    // Now built-in modules:
    ASSERT(treeViewAdd_usingStringArray(tree, &iter, qsBuiltInBlocks));

    // Now get block modules into the tree view from the
    // environment variable "QS_BLOCK_PATH" if it is set:
    char *path;
    if((path = getenv("QS_BLOCK_PATH"))) {

        // Make a copy (cp) that we will edit a little.
        char *cp = strdup(path);
        ASSERT(cp, "strdup() failed");

        // c is a dummy char iterator.
        char *c = cp;
        // Use path as a dummy string iterator.
        path = c;
        // iterate through the characters.
        while(*c) {
            // TODO: Linux specific code using ':' path separator:
            //
            // Remove surplus ':'
            while(*c == ':') {
                *c = '\0';
                ++c;
                if(*c) path = c;
                else break;
            }
            while(*c && *c != ':') ++c;
        
            while(*c == ':') {
                // zero terminate the copies sub-string.
                *c = '\0';
                ++c;
            }
            if(*path) {
                // We use the "real" full path if there is one.
                char *realPath = realpath(path, 0);
                if(realPath) {
                    treeViewAdd(tree, realPath, path/*tree label*/,
                            0, CheckFile, GetName);
                    free(realPath);
                }
                // Go to next path string if there be one:
                path = c;
            }
        }
        // free the copy.
        free(cp);
    }

    treeViewShow(tree, "Choose Block");
    g_signal_connect(tree, "row-activated",
            G_CALLBACK(TreeView_Select_cb), w);

    w->treeView = GTK_TREE_VIEW(tree);

    gtk_container_add(GTK_CONTAINER(sw), tree);
    gtk_widget_show(tree);
}


void
RecreateTreeView(struct Window *w) {

    DSPEW("Reloading block tree view");

    GtkWidget *parent = gtk_widget_get_parent(GTK_WIDGET(w->treeView));

    gtk_widget_destroy(GTK_WIDGET(w->treeView));

    CreateTreeView(GTK_CONTAINER(parent), w);
}
