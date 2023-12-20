// Stupid-ass GTK3 tree view widget wrapper/API.

extern
GtkWidget *treeViewCreate(void);

extern
bool treeViewAdd(GtkWidget *tree,
        const char *path,  const char *name, GtkTreeIter *topIter,
        bool (*CheckFile)(int dirfd, const char *path),
        char *(*GetName)(int dirfd, const char *path));

extern
bool treeViewAdd_usingStringArray(
        GtkWidget *tree,  GtkTreeIter *parentIter,
        const char * const builtInBlocks[]);


extern
void treeViewShow(GtkWidget *tree, const char *header);


extern
void ShowTreeViewPopupMenu(char *newPath, char *dir);
