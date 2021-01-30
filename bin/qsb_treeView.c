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
#include <gdk/gdkevents.h>
#include <gdk/gdkx.h>

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


// This will close the fd.
//
bool CheckView(const char *path,  const char *name,
        int dirfd, bool (*CheckFile)(int dirfd, const char *path)) {

    if(!name) name = path;

    if(path && CheckFile(dirfd, path))
        return true;

    DIR *dir = fdopendir(dirfd);
    if(!dir) return false;

    struct dirent *ent = readdir(dir);

    while(ent) {

        switch(ent->d_type) {
            case DT_REG:

                if(CheckFile(dirfd, ent->d_name)) {
                    closedir(dir);
                    return true;
                }
                break;
            case DT_DIR:
                 if(strcmp(ent->d_name, ".") == 0 ||
                        strcmp(ent->d_name, "..") == 0) break;
            {
                int fd = openat(dirfd, ent->d_name, 0);
                if(fd >= 0) {
                    closedir(dir);
                    return CheckView(ent->d_name, 0, fd, CheckFile);
                }
            }
                break;
        }
        ent = readdir(dir);
    }
    closedir(dir);
    return false;
}


static
void AddView(GtkTreeStore *store, const char *path,  const char *name,
        int dirfd, GtkTreeIter *parentIt,
        bool (*CheckFile)(int dirfd, const char *path),
        char * (*GetName)(int dirfd, const char *path)) {

    if(!name) name = path;

    DIR *dir = fdopendir(dirfd);
    if(!dir) {
        close(dirfd);
        return;
    }

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
                int fd = openat(dirfd, ent->d_name, 0);
                if(fd >= 0) {
                    if(CheckView(ent->d_name, 0, fd, CheckFile)) {

                        GtkTreeIter iter;
                        gtk_tree_store_append(store, &iter, parentIt);
                        gtk_tree_store_set(store, &iter, 0,
                                ent->d_name, -1);
                        // CheckView() closed it.  We open it again.
                        fd = openat(dirfd, ent->d_name, 0);
                        if(fd >= 0) 
                            AddView(store, ent->d_name, 0, fd, &iter,
                                    CheckFile, GetName);
                    }
                }
            }
                break;
        }
        ent = readdir(dir);
    }
    closedir(dir);
}


// Returns true if we found and made some entries.
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

    bool ret = CheckView(path, name, fd, CheckFile);
    if(ret) {

        fd = open(path, 0);
        if(fd <= 0) {
            errno = 0;
            return false;
        }

        GtkTreeIter iter;
        gtk_tree_store_append(store, &iter, 0);
        gtk_tree_store_set(store, &iter, 0, name, -1);

        AddView(store, path, name, fd, &iter, CheckFile, GetName);
    }

    return ret;
}


void treeViewShow(GtkWidget *tree, const char *header) {

    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();

    GtkTreeViewColumn *column =
        gtk_tree_view_column_new_with_attributes (header,
                                                   renderer,
                                                   "text", 0,
                                                   NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

    gtk_widget_show(tree);
}
