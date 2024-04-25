// Qt has a generic tab-bar QTabBar which has no notebook page like widget
// associated with each tab.  The pre-made tab-notebook thing is managed
// with a QTabWidget object.  Me, coming from a GTK widgets background,
// the Qt tabs terminology seems a little off.
#include <QtCore/QObject>
#include <QtWidgets/QtWidgets>

#include "../lib/debug.h"


class Tabs : private QTabWidget
{
    Q_OBJECT

public:
    explicit Tabs(QWidget *parent);
    ~Tabs(void);
    void AddTab(const char *name);

private slots:
    void closeTab(const int& index);
};


// There will be just one Qt tab thingy per process.
static Tabs *tabs = 0;

void Tabs::closeTab(const int& index) {

    delete widget(index);
}


Tabs::Tabs(QWidget *parent) :
    QTabWidget(parent) {
    setTabsClosable(true);
    connect(this, SIGNAL(tabCloseRequested(int)),
            this, SLOT(closeTab(int)));
    show();
}

Tabs::~Tabs(void) {

    ERROR();
}


void Tabs::AddTab(const char *name) {

    ASSERT(tabs);

    QTextEdit *textedit = new QTextEdit(QString("text ") + name, tabs);
    textedit->show();
    tabs->addTab(textedit, tr(name));
}

void AddTab(const char *name, QWidget *parent) {

    if(!tabs) {
        ASSERT(parent);
        tabs = new Tabs(parent);
    }

    tabs->AddTab(name);
}

