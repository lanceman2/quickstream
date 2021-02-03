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
#include <sys/types.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <gtk/gtk.h>

#include "../lib/debug.h"


GtkWidget *treeViewCreate(GtkWidget *tree) {

    if(!tree)
        tree = gtk_tree_view_new();

    GtkTreeStore *store =
        gtk_tree_store_new(1, /*number of columns*/
            G_TYPE_STRING/*path*/);

    gtk_tree_view_set_model(GTK_TREE_VIEW(tree),
            GTK_TREE_MODEL(store));

    return tree;
}


// This returns true if a file is found that passes the CheckFile() call
// in a recursive call of this function.
//
// if(path) is set we just check if the path passes the CheckFile() test.
// else if(!path) we check all the directories there in dirfd.
//
// This will close the dirfd.
//
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
        // We have a problem, Houston.
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
                    gtk_tree_store_append(store, &iter, parentIt);
                    gtk_tree_store_set(store, &iter, 0, name, -1);
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
                        gtk_tree_store_append(store, &iter, parentIt);
                        gtk_tree_store_set(store, &iter, 0,
                                ent->d_name, -1);
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


// Returns true if we found and made some entries.
//
// path is a path to a directory that we can open.
//
bool treeViewAdd(GtkWidget *tree,
        const char *path,  const char *name,
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

    const uint32_t recurseMax = 20;

    bool ret = CheckDir(fd, CheckFile, recurseMax);
    if(ret) {

        // CheckDir() close the fd so we need to open it again.
        fd = open(path, 0);
        if(fd <= 0) {
            errno = 0;
            return false;
        }

        GtkTreeIter iter;
        gtk_tree_store_append(store, &iter, 0);
        gtk_tree_store_set(store, &iter, 0, name, -1);

        AddView(store, path, name, fd, &iter, CheckFile, GetName,
                recurseMax);
    }

    return ret;
}


void treeViewShow(GtkWidget *tree, const char *header) {

    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();

    GtkTreeViewColumn *column =
        gtk_tree_view_column_new_with_attributes(header,
                                        renderer,
                                        "text", 0,
                                        NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

    gtk_widget_show(tree);
}
