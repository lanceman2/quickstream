
class Notebook;

extern Notebook *CreateNotebook(QWidget *parent);

extern void Notebook_AddTab(Notebook *notebook,
        const char *name, const char *blockPath=0);

