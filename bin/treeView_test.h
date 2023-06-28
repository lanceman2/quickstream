
extern
GtkWidget *treeViewCreate(void);

extern
GtkWidget *treeViewAdd(GtkWidget *tree,
        const char *path,  const char *name,
        bool (*CheckFile)(int dirfd, const char *path),
        char *(*GetName)(int dirfd, const char *path));

extern
void treeViewShow(GtkWidget *tree, const char *header);
