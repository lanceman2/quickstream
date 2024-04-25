// Most of this file was taken from:
// qtbase/examples/widgets/itemviews/simpletreemodel/
// from in the qt6 source.
//
// I got the qt6 source using instructions at
// https://wiki.qt.io/Building_Qt_6_from_Git
//
// The treeview widget is a pain (complex) in both GTK3 and Qt6.
// Nether GTK3 or Qt6 have a simple treeview example C or C++ code that is
// close to this use case.  Treeview widgets may just be inherently
// complex.  We could not use the generic file treeview.  It just did
// far more than we needed, and did not have a simple way to opt out
// of all the built-in file browser things.

#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <libgen.h>

#include <QtCore/QObject>
#include <QtWidgets/QtWidgets>

#include "../include/quickstream.h"

#include "../lib/debug.h"

#include "qsQt_treeview.h"



using namespace Qt::StringLiterals;



class TreeItem;

class TreeModel : public QAbstractItemModel {

    Q_OBJECT

public:
    //Q_DISABLE_COPY_MOVE(TreeModel)

    explicit TreeModel(QObject *parent = nullptr);
    ~TreeModel() override;

    QVariant data(const QModelIndex &index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    QModelIndex index(int row, int column,
                      const QModelIndex &parent = {}) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;

    void addBlockDirectory(const char *path, const char *label);

private:

    // Adds either a directory or a file.
    bool AddPaths(TreeItem *parent, int dirfd,
            const char *path, const char *label, int depth);

    // Adds just one item in the tree view.
    TreeItem *AddItem(TreeItem *parent, const char *path, const char *label);

    TreeItem *rootItem;
};


QTreeView *MakeTreeview(QWidget *parent) {

    QTreeView *view = new QTreeView(parent);
    TreeModel *model = new TreeModel(view);

    view->setModel(model);
    view->setWindowTitle(TreeModel::tr("Simple Tree Model"));

    for(int c = 0; c < model->columnCount(); ++c)
        view->resizeColumnToContents(c);
    //view.expandAll();
    view->expandToDepth(0);

    return view;
}




class TreeItem {

public:
    explicit TreeItem(QVariantList data,
            const char *path, TreeItem *parentItem = nullptr);
    ~TreeItem(void);

    void appendChild(TreeItem *child);

    TreeItem *child(int row);
    int childCount() const;
    int columnCount() const;
    QVariant data(int column) const;
    int row() const;
    TreeItem *parentItem();

    inline void sortChildrensChildren(void) {
        for(auto & child: m_childItems)
            (*child).sortChildren();
    }

    inline void sortChildren(void) {
        std::sort(m_childItems.begin(), m_childItems.end(),
            [] (const auto& lhs, const auto& rhs) {
#if 1
                return (strcoll((*lhs).path, (*rhs).path) > 0) ? false: true;
            });
#else // I can't get a sort that puts files before directories; this does
      // not work:
        std::sort(m_childItems.begin(), m_childItems.end(),
            [] (const auto& lhs, const auto& rhs) {
                if(((*lhs).isFile() && (*rhs).isFile()) ||
                        !((*lhs).isFile() && !(*rhs).isFile()))
                    return (strcoll((*lhs).path, (*rhs).path) > 0) ? false: true;

                DASSERT(((*lhs).isFile() && !(*rhs).isFile()) ||
                        (!(*lhs).isFile() && (*rhs).isFile()));

                if((*lhs).isFile())
                    return true;
                else
                    return false;
            });
#endif
        for(auto & child: m_childItems)
            (*child).sortChildren();
    }

    inline void removeChildren(void) {

        auto it = m_childItems.rbegin();
        while(it != m_childItems.rend()) {
            TreeItem *child = *it;
            m_childItems.pop_back();
            it = m_childItems.rbegin();
            delete child;
        }
        DASSERT(m_childItems.empty());
    }

    inline void removeChild(TreeItem *child) {

        for(std::vector<TreeItem *>::iterator  it = m_childItems.begin();
                it != m_childItems.end(); ++it)
            if(*it == child) {
                m_childItems.erase(it);
                break;
            }
    }

    inline const char *getPath(void) const {
        return (const char *) path;
    }

    inline size_t numChildren(void) const {
        return m_childItems.size();
    }


    inline bool isFile(void) const {
        // This is either a directory with children or a file with no
        // children.
        return (m_childItems.empty()) ? true: false;
    };

private:
    std::vector<TreeItem *> m_childItems;
    QVariantList m_itemData;
    TreeItem *m_parentItem;
    char *path;
};



TreeItem::~TreeItem(void) {

    //DSPEW("Destroying %s", path);

    // Destroy children:
    removeChildren();

    if(m_parentItem)
        m_parentItem->removeChild(this);

    if(path) {
#ifdef DEBUG
        memset(path, 0, strlen(path));
#endif
        free(path);
        path = 0;
    }
}


TreeItem::TreeItem(QVariantList data,
        const char *path_in, TreeItem *parent)
    : m_itemData(std::move(data)), m_parentItem(parent) {


#ifdef DEBUG
    if(parent) {
        DASSERT(path_in);
        DASSERT(path_in[0]);
    }
#endif

    if(path_in) {
        path = strdup(path_in);
        ASSERT(path, "strdup() failed");
    } else {
        path = 0;
    }

    if(parent)
        parent->appendChild(this);
}


void TreeItem::appendChild(TreeItem *chiLd) {
    DASSERT(chiLd);
    m_childItems.push_back(chiLd);
}


TreeItem *TreeItem::child(int row) {
    return row >= 0 && row < childCount() ?
        m_childItems.at(row) : nullptr;
}

int TreeItem::childCount() const {
    return int(m_childItems.size());
}

int TreeItem::columnCount() const {
    return int(m_itemData.count());
}

QVariant TreeItem::data(int column) const {
    return m_itemData.value(column);
}

TreeItem *TreeItem::parentItem() {
    return m_parentItem;
}

int TreeItem::row() const
{
    if (m_parentItem == nullptr)
        return 0;
    const auto it = std::find_if(m_parentItem->m_childItems.cbegin(),
            m_parentItem->m_childItems.cend(),
                    [this](const TreeItem *treeItem) {
                                return treeItem == this;
                    });

    if (it != m_parentItem->m_childItems.cend())
        return std::distance(m_parentItem->m_childItems.cbegin(), it);
    ASSERT(false); // should not happen
    return -1;
}



TreeItem *TreeModel::AddItem(TreeItem *parent, const char *path, const char *label) {

    DASSERT(parent);

    QVariantList columnData;
    columnData.reserve(1);
    columnData << label;

    //DSPEW("Adding \"%s\"", path);

    return new TreeItem(columnData, path, parent);
}



// If non-zero the returned value must be freed.
static char *CheckFile(const char *name) {

    // return true to add block based on the file name.

    // Skip file names starting with "_" and "."
    //
    // TODO: add this valid file names to documentation.
    if(name[0] == '.' || name[0] == '_')
        return 0;

    size_t len = strlen(name);
    if(len < 3)
        return 0;

    char *ret = 0;

    if(name[len-3] == '.') {
        if(name[len-2] == 's' && name[len-1] == 'o') {
            // Name matches glob *.so
            ret = strdup(name);
            ASSERT(ret, "strdup() failed");
            return ret;
        }
        if(name[len-2] == 'b' && name[len-1] == 'i') {
            // Name matches glob *.bi, a built-in module.
            ret = (char *) calloc(1, len -2);
            ASSERT(ret, "calloc(1,%zu) failed", len-2);
            strncpy(ret, name, len-3);
            return ret;
        }
    }

    //fprintf(stderr, "%s\n", name);

    return 0;
}


static bool CheckDir(const char *name) {

    // Note: we do allow having the directory file named ".." and "."

    // We do not allow the directory file named some thing like .[A-Za-z]*
    // so it skips directories like .git/ and .config/
    //
    if(name[0] == '.' && name[1] >= 'A' && name[1] <= 'z')
        return false;

    return true;
}



// depth:  starts at 0 at the root directory as we define it.
//
// Returns true if there is a file (leaf node) in somewhere in
// the file path being added.
//
bool TreeModel::AddPaths(TreeItem *parent, int dirfd,
        const char *path, const char *label, int depth) {

    DASSERT(parent);

    if(depth > 200)
        // We have a loop in this directory? Is that even possible? In any
        // case that is far enough.  We don't need to blowup the function
        // call stack.  Having this deep (deeper) of a sub-directory would
        // be a total pain in the ass for the user.
        return false;


    struct stat statbuf;

    if(dirfd > -1) {
        if(0 != fstatat(dirfd, path, &statbuf, 0)) {
            DSPEW("stat(\"%s\",) failed", path);
            return false;
        }
    } else if(0 != stat(path, &statbuf)) {
        DSPEW("stat(\"%s\",) failed", path);
        return false;
    }


    if((statbuf.st_mode & S_IFMT) == S_IFDIR) {

        // This path is a directory.
        int fd;

        errno = 0;

        if(dirfd > -1)
            fd = openat(dirfd, path, O_RDONLY | O_DIRECTORY);
        else
            fd = open(path, O_RDONLY | O_DIRECTORY);

        if(fd < 0) {
            if(dirfd > -1)
                ERROR("openat(%d,\"%s\",,) failed", dirfd, path); 
            else
                ERROR("open(\"%s\",,) failed", path);
            return false;
        }

        DIR *dir = fdopendir(fd);

        if(!dir) {
            DSPEW("fdopendir(%d) path \"%s\" failed", fd, path);
            close(fd);
            return false;
        }

        //fprintf(stderr, "Maybe Adding directory: %s\n", path);

        bool gotOne = false;

        if(CheckDir(path)) {

            // ADD a directory.
            TreeItem *item = AddItem(parent, path, label);

            struct dirent *ent;
            while((ent = readdir(dir))) {
                if(strcmp(ent->d_name, "..") != 0 &&
                        strcmp(ent->d_name, ".") != 0) {
                    if(AddPaths(item, fd, ent->d_name, ent->d_name, depth + 1))
                        gotOne = true;
                }
            }

            if(!gotOne) {
                // We added this directory for nothing, so now we start
                // removing it, by removing children in it.  The parent
                // directory will remove this after we return from here.
                //
                // We are doing this in one pass through the directories,
                // but if we did this in two passes we may have not needed
                // to add this directory to begin with which would require
                // that we store more state than just this state that we
                // have in the function call stack.  This seems to be the
                // simplest way to code this.
                //DSPEW("Removing empty directory %s ==> \"%s\"",
                //path, item->getPath());

                delete item;
            }
        }

        if(closedir(dir))
            ERROR("closedir() failed for \"%s\"", path);

        close(fd);

        return gotOne;

    } else if((statbuf.st_mode & S_IFMT) == S_IFREG) {

        // This path is a regular file.
        char *name = CheckFile(path);
        if(name) {
            // ADD a block.
            AddItem(parent, name, name);
            //DSPEW("Adding file: %s", name);
#ifdef DEBUG
            memset(name, 0, strlen(name));
#endif
            free(name);
            return true; // got one
        }
    }

    return false;
}


void TreeModel::addBlockDirectory(const char *path, const char *label) {

    DSPEW("Adding block directory \"%s\" from directory %s",
            label, path);

    AddPaths(rootItem, -1, path, label, 0);
}


TreeModel::TreeModel(QObject *parent)
    : QAbstractItemModel(parent) {

    rootItem = new TreeItem(QVariantList{tr("Blocks")}, 0);

    addBlockDirectory(qsBlockDir, "Core");

    // We should have some Blocks in the tree view.
    DASSERT(rootItem->childCount() > 0);
    DASSERT(rootItem->child(0));

    char *env = getenv("QS_BLOCK_PATH");
    if(env && env[0]) {
        char *e = strdup(env);
        ASSERT(e, "strdup() failed");
        size_t len = strlen(e);
        char *path = e + len - 1;
        // TODO: This is UNIX specific code.  Using ':' as the directory
        // separator.
        while(path > e) {
            while(path > e && *path == ':') {
                *path = '\0';
                --path;
            }
            while(path > e && *path != ':') {
                --path;
            }
            if(*path == ':')
                ++path;
            if(path[0]) {
                addBlockDirectory(path, path);
            }
            if(path > e)
                --path;
        }

#ifdef DEBUG
        memset(e, 0, len);
#endif
        free(e);
    }

    // At this point you may be weary of file leaks, given we just opened
    // over 100 (maybe 1000) files.  As of this time, 2024 Feb 22, I see
    // just the bunch of stupid files from Qt.  I wonder why a GUI needs
    // so many files (about 30).  I could understand needing say 5 files.
    // It's most likely just Qt programmers being stupid and lazy; adding
    // crap that we'll never use in this program.  Looks like Qt keeps all
    // it's files when we make a new QApplication after destroying the old
    // one.  Looks kind-of like the QApplication is a singleton object
    // that exists in a "hidden form", not defined by it's user interface.
    // If you try to make 2 QApplication objects, in one process, at one
    // time, the program crashes.  Seems to me they should have at least
    // made that case have a failure mode of some kind, and not just let
    // the program crash.  It's still greatly better than what GTK3 does
    // when we try to make consecutive gtk_init() objects.
    //
    // We do not want to change the order of the first generation of
    // children, so that the user gets them listed in the order that they
    // required them.  We just sort each sub-tree and below.
    rootItem->sortChildrensChildren();
}


TreeModel::~TreeModel() {

    DASSERT(rootItem);

    delete rootItem;
}


int TreeModel::columnCount(const QModelIndex &parent) const {

    if(parent.isValid())
        return static_cast<TreeItem*>(parent.internalPointer())->columnCount();
    return rootItem->columnCount();
}

QVariant TreeModel::data(const QModelIndex &index, int role) const {

    if (!index.isValid() || role != Qt::DisplayRole)
        return {};

    const auto *item = static_cast<const TreeItem*>(index.internalPointer());
    return item->data(index.column());
}

Qt::ItemFlags TreeModel::flags(const QModelIndex &index) const {

    return index.isValid()
        ? QAbstractItemModel::flags(index):
        Qt::ItemFlags(Qt::NoItemFlags);
}

QVariant TreeModel::headerData(int section, Qt::Orientation orientation,
                               int role) const {
    return orientation == Qt::Horizontal && role == Qt::DisplayRole
        ? rootItem->data(section) : QVariant{};
}

QModelIndex TreeModel::index(int row, int column,
        const QModelIndex &parent) const {

    if(!hasIndex(row, column, parent))
        return {};

    TreeItem *parentItem = parent.isValid()
        ? static_cast<TreeItem*>(parent.internalPointer())
        : rootItem;

    if (auto *childItem = parentItem->child(row))
        return createIndex(row, column, childItem);
    return {};
}

QModelIndex TreeModel::parent(const QModelIndex &index) const {

    if(!index.isValid())
        return {};

    auto *childItem = static_cast<TreeItem*>(index.internalPointer());
    TreeItem *parentItem = childItem->parentItem();

    return parentItem != rootItem
        ? createIndex(parentItem->row(), 0, parentItem) : QModelIndex{};
}

int TreeModel::rowCount(const QModelIndex &parent) const {

    if (parent.column() > 0)
        return 0;

    const TreeItem *parentItem = parent.isValid()
        ? static_cast<const TreeItem*>(parent.internalPointer())
        : rootItem;

    return parentItem->childCount();
}
