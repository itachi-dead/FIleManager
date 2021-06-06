#include "MainWindow.h"
#include "Pane.h"
#include "PreferencesDialog.h"
#include "Properties.h"

#include <QActionGroup>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle(tr("File Manager"));
    setUnifiedTitleAndToolBarOnMac(true);
    createActionsAndMenus();

    settings = new QSettings("Dima Melkunas", "File Manager");
    splitter = new QSplitter(this);

    fileSystemModel = new QFileSystemModel;
    fileSystemModel->setFilter(QDir::NoDot | QDir::AllEntries | QDir::System);
    fileSystemModel->setRootPath("");
    fileSystemModel->setReadOnly(false);

    fileSystemProxyModel = new FileSystemModelFilterProxyModel();
    fileSystemProxyModel->setSourceModel(fileSystemModel);
    fileSystemProxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    //fileSystemProxyModel->sort(0);//////////////////////////////////////

    directoryTreeView = new QTreeView(splitter);
    directoryTreeView->setModel(fileSystemProxyModel);
    directoryTreeView->setHeaderHidden(true);
    directoryTreeView->setUniformRowHeights(true);
    directoryTreeView->hideColumn(1);
    directoryTreeView->hideColumn(2);
    directoryTreeView->hideColumn(3);
    directoryTreeView->setDragDropMode(QAbstractItemView::DropOnly);
    directoryTreeView->setDefaultDropAction(Qt::MoveAction);
    directoryTreeView->setDropIndicatorShown(true);
    directoryTreeView->setEditTriggers(QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);
    directoryTreeView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(directoryTreeView, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(showContextMenu(const QPoint&)));
    treeSelectionModel = directoryTreeView->selectionModel();
    connect(treeSelectionModel, SIGNAL(currentChanged(QModelIndex, QModelIndex)), this, SLOT(treeSelectionChanged(QModelIndex, QModelIndex)));

    leftPane = new Pane(splitter);
    rightPane = new Pane(splitter);

    connect(qApp, SIGNAL(focusChanged(QWidget*, QWidget*)), SLOT(focusChangedSlot(QWidget*, QWidget*)));

    splitter->addWidget(directoryTreeView);
    splitter->addWidget(leftPane);
    splitter->addWidget(rightPane);
    splitter->setHandleWidth(3);
    this->setCentralWidget(splitter);
    connect(QApplication::clipboard(), SIGNAL(changed(QClipboard::Mode)), this, SLOT(clipboardChanged()));
    restoreState();
}

void MainWindow::focusChangedSlot(QWidget *, QWidget *now)
{
    if (now == rightPane->pathLineEdit || now == rightPane->treeView || now == rightPane->listView)
        setActivePane(rightPane);
    else if (now == leftPane->pathLineEdit || now == leftPane->treeView || now == leftPane->listView)
        setActivePane(leftPane);
}

void MainWindow::setActivePane(Pane* pane)
{
    pane->setActive(true);
    if (pane == leftPane)
        rightPane->setActive(false);
    else
        leftPane->setActive(false);
    activePane = pane;
    updateViewActions();
}

Pane* MainWindow::getActivePane()
{
    return(activePane);
}

void MainWindow::treeSelectionChanged(QModelIndex current, QModelIndex previous)
{
    QFileInfo fileInfo(fileSystemModel->fileInfo(fileSystemProxyModel->mapToSource(current)));
    if(!fileInfo.exists())
        return;
    getActivePane()->moveTo(fileInfo.filePath());
}

void MainWindow::moveTo(const QString path)
{
    QModelIndex index = fileSystemProxyModel->mapFromSource(fileSystemModel->index(path));
    treeSelectionModel->select(index, QItemSelectionModel::Select);
    getActivePane()->moveTo(path);
}

void MainWindow::showContextMenu(const QPoint& position)
{
    contextMenu->exec(directoryTreeView->mapToGlobal(position));
}

void MainWindow::cut()
{
    QModelIndexList selectionList;

    QWidget* focus(focusWidget());
    QAbstractItemView* view;
    if (focus == directoryTreeView || focus == leftPane->treeView || focus == leftPane->listView || focus == rightPane->treeView || focus == rightPane->listView){
        view =  qobject_cast<QAbstractItemView *>(focus);
        selectionList = view->selectionModel()->selectedIndexes();
    }

    if(selectionList.count() == 0)
        return;

    QApplication::clipboard()->setMimeData(fileSystemModel->mimeData(selectionList));
    pasteAction->setData(true);

    view->selectionModel()->clear();
}

void MainWindow::copy()
{
    QModelIndexList selectionList;

    QWidget* focus(focusWidget());
    QAbstractItemView* view;
    if (focus == directoryTreeView || focus == leftPane->treeView || focus == leftPane->listView || focus == rightPane->treeView || focus == rightPane->listView){
        view =  qobject_cast<QAbstractItemView *>(focus);
        selectionList = view->selectionModel()->selectedIndexes();
    }

    if(selectionList.count() == 0)
        return;

    QApplication::clipboard()->setMimeData(fileSystemModel->mimeData(selectionList));
    pasteAction->setData(false);
#ifdef __HAIKU__
    pasteAction->setEnabled(true);
#endif
}

void MainWindow::paste()
{
    QWidget* focus(focusWidget());
    Qt::DropAction cutOrCopy(pasteAction->data().toBool() ? Qt::MoveAction : Qt::CopyAction);

    if (focus == leftPane->treeView || focus == leftPane->listView || focus == rightPane->treeView || focus == rightPane->listView){
        fileSystemModel->dropMimeData(QApplication::clipboard()->mimeData(), cutOrCopy, 0, 0, qobject_cast<QAbstractItemView *>(focus)->rootIndex());
    } else if (focus == directoryTreeView){
        fileSystemModel->dropMimeData(QApplication::clipboard()->mimeData(), cutOrCopy, 0, 0,  fileSystemProxyModel->mapToSource(directoryTreeView->currentIndex()));
    }
}

void MainWindow::del()
{
    QModelIndexList selectionList;
    bool yesToAll = false;
    bool ok = false;
    bool confirm = true;

    QWidget* focus(focusWidget());
    QAbstractItemView* view;
    if (focus == directoryTreeView || focus == leftPane->treeView || focus == leftPane->listView || focus == rightPane->treeView || focus == rightPane->listView){
        view = qobject_cast<QAbstractItemView *>(focus);
        selectionList = view->selectionModel()->selectedIndexes();
    }

    for(int i = 0; i < selectionList.count(); ++i)
    {
        QFileInfo file(fileSystemModel->filePath(selectionList.at(i)));
        if(file.isWritable())
        {
            if(file.isSymLink()) ok = QFile::remove(file.filePath());
            else
            {
                if(!yesToAll)
                {
                    if(confirm)
                    {
                        int answer = QMessageBox::information(this, tr("Удалить файл"), "Вы уверены хотите удалить <p><b>\"" + file.filePath() + "</b>?",QMessageBox::Yes | QMessageBox::No | QMessageBox::YesToAll);
                        if(answer == QMessageBox::YesToAll)
                            yesToAll = true;
                        if(answer == QMessageBox::No)
                            return;
                    }
                }
                ok = fileSystemModel->remove(selectionList.at(i));
            }
        }
        else if(file.isSymLink())
            ok = QFile::remove(file.filePath());
    }

    if(!ok)
        QMessageBox::information(this, tr("Не удалено"), tr("Некоторые файлы не могут быть удалены."));
}

void MainWindow::newFolder()
{
    QAbstractItemView* currentView = qobject_cast<QAbstractItemView *>(getActivePane()->stackedWidget->currentWidget());
    QModelIndex newDir = fileSystemModel->mkdir(currentView->rootIndex(), QString("Новая папка"));

    if (newDir.isValid()) {
        currentView->selectionModel()->setCurrentIndex(newDir, QItemSelectionModel::ClearAndSelect);
        currentView->edit(newDir);
    }
}

void MainWindow::clipboardChanged()
{
    if(QApplication::clipboard()->mimeData()->hasUrls())
        pasteAction->setEnabled(true);
    else
    {
        pasteAction->setEnabled(false);
    }
}

void MainWindow::toggleToDetailView()
{
    getActivePane()->setViewTo(Pane::TreeViewMode);
}

void MainWindow::toggleToIconView()
{
    getActivePane()->setViewTo(Pane::ListViewMode);
}

void MainWindow::toggleHidden()
{
    if(hiddenAction->isChecked())
        fileSystemModel->setFilter(QDir::NoDot | QDir::AllEntries | QDir::System | QDir::Hidden);
    else
        fileSystemModel->setFilter(QDir::NoDot | QDir::AllEntries | QDir::System);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveState();
    event->accept();
}

void MainWindow::createActionsAndMenus()
{
    deleteAction = new QAction(QIcon::fromTheme("edit-delete", QIcon(":/Images/Delete.ico")), tr("Удалить"), this );
    deleteAction->setStatusTip(tr("Удалить файл"));
    deleteAction->setShortcut(QKeySequence::Delete);
    connect(deleteAction, SIGNAL(triggered()), this, SLOT(del()));

    newFolderAction = new QAction(QIcon::fromTheme("edit-new", QIcon(":/Images/NewFolder.ico")), tr("Новая папка"), this );
    newFolderAction->setStatusTip(tr("Создать новую папку"));
    newFolderAction->setShortcut(QKeySequence::New);
    connect(newFolderAction, SIGNAL(triggered()), this, SLOT(newFolder()));

    preferencesAction = new QAction(QIcon::fromTheme("preferences-other", QIcon(":/Images/Preferences.ico")), tr("&Параметры"), this );
    preferencesAction->setStatusTip(tr("Задать параметры"));
    preferencesAction->setShortcut(QKeySequence::Preferences);
    connect(preferencesAction, SIGNAL(triggered()), this, SLOT(showPreferences()));

    exitAction = new QAction(QIcon::fromTheme("application-exit", QIcon(":/Images/Exit.png")), tr("&Выход"), this );
    //exitAction->setShortcuts(QKeySequence::QuitRole);//////////////////////////////////
    exitAction->setMenuRole(QAction::QuitRole);
    exitAction->setStatusTip(tr("Выйти из File Manager"));
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, SIGNAL(triggered()), this, SLOT(close()));

    cutAction = new QAction(QIcon::fromTheme("edit-cut", QIcon(":/Images/Cut.png")), tr("Вырезать"), this );
    cutAction->setStatusTip(tr("Вырезать файл"));
    cutAction->setShortcut(QKeySequence::Cut);
    connect(cutAction, SIGNAL(triggered()), this, SLOT(cut()));

    copyAction = new QAction(QIcon::fromTheme("edit-copy", QIcon(":/Images/Copy.png")), tr("Копировать"), this );
    copyAction->setStatusTip(tr("Копировать файл"));
    copyAction->setShortcut(QKeySequence::Copy);
    connect(copyAction, SIGNAL(triggered()), this, SLOT(copy()));

    pasteAction = new QAction(QIcon::fromTheme("edit-paste", QIcon(":/Images/Paste.png")), tr("Вставить"), this );
    pasteAction->setStatusTip(tr("Вставить файл"));
    pasteAction->setEnabled(false);
    pasteAction->setShortcut(QKeySequence::Paste);
    connect(pasteAction, SIGNAL(triggered()), this, SLOT(paste()));

    detailViewAction = new QAction(QIcon::fromTheme("view-list-details", QIcon(":/Images/DetailView.ico")), tr("Таблица"), this );
    detailViewAction->setStatusTip(tr("Табличный вид"));
    detailViewAction->setCheckable(true);
    connect(detailViewAction, SIGNAL(triggered()), this, SLOT(toggleToDetailView()));

    iconViewAction = new QAction(QIcon::fromTheme("view-list-icons", QIcon(":/Images/IconView.ico")), tr("Список"), this );
    iconViewAction->setStatusTip(tr("Список"));
    iconViewAction->setCheckable(true);
    connect(iconViewAction, SIGNAL(triggered()), this, SLOT(toggleToIconView()));

    hiddenAction = new QAction(QIcon::fromTheme("folder-saved-search"), tr("Скрытые файлы"), this );
    hiddenAction->setStatusTip(tr("Показывать скрытые файлы"));
    hiddenAction->setCheckable(true);
    connect(hiddenAction, SIGNAL(triggered()), this, SLOT(toggleHidden()));

    viewActionGroup = new QActionGroup(this);
    viewActionGroup->addAction(detailViewAction);
    viewActionGroup->addAction(iconViewAction);

    aboutAction = new QAction(QIcon::fromTheme("help-about", QIcon(":/Images/About.ico")), tr("&О программе"), this );
    aboutAction->setStatusTip(tr("О программе"));
    aboutAction->setMenuRole (QAction::AboutRole);
    connect(aboutAction, SIGNAL(triggered()), this, SLOT(showAboutBox()));

    propertiesAction = new QAction(QIcon::fromTheme("document-properties", QIcon(":/Images/Properties.ico")), tr("&Свойства"), this );
    propertiesAction->setStatusTip(tr("Свойства"));
    propertiesAction->setShortcut(Qt::CTRL + Qt::Key_R);
    connect(propertiesAction, SIGNAL(triggered()), this, SLOT(showProperties()));

    menuBar = new QMenuBar(0);

    fileMenu = menuBar->addMenu(tr("&Файл"));
    fileMenu->addAction(newFolderAction);
    fileMenu->addAction(deleteAction);
    fileMenu->addAction(preferencesAction);
    fileMenu->addSeparator();
    fileMenu->addAction(exitAction);

    editMenu = menuBar->addMenu(tr("&Правка"));
    editMenu->addAction(cutAction);
    editMenu->addAction(copyAction);
    editMenu->addAction(pasteAction);

    viewMenu = menuBar->addMenu(tr("&Вид"));
    viewMenu->addAction(detailViewAction);
    viewMenu->addAction(iconViewAction);
    viewMenu->addAction(hiddenAction);

    helpMenu = menuBar->addMenu(tr("&Справка"));
    helpMenu->addAction(aboutAction);

    setMenuBar(menuBar);

    toolBar = addToolBar(tr("Main"));

    contextMenu = new QMenu(this);
    contextMenu->addAction(detailViewAction);
    contextMenu->addAction(iconViewAction);
    contextMenu->addAction(propertiesAction);
    contextMenu->addSeparator();
    contextMenu->addAction(cutAction);
    contextMenu->addAction(copyAction);
    contextMenu->addAction(pasteAction);
    contextMenu->addSeparator();
    contextMenu->addAction(deleteAction);

    toolBar->addAction(detailViewAction);
    toolBar->addAction(iconViewAction);
}

void MainWindow::saveState()
{
    settings->setValue("Geometry", saveGeometry());
//    settings->setValue("ShowStatusBar", this->statusBar->isVisible());/////////////////////////////////////
    settings->setValue("ShowStatusBar", statusBar()->isVisible());
    settings->setValue("ShowToolBar", toolBar->isVisible());

    settings->setValue("MainSplitterSizes", splitter->saveState());
    settings->setValue("LeftPaneActive", leftPane->isActive());

    settings->setValue("LeftPanePath", leftPane->pathLineEdit->text());
    settings->setValue("LeftPaneFileListHeader", leftPane->treeView->header()->saveState());
    settings->setValue("LeftPaneViewMode", leftPane->stackedWidget->currentIndex());

    settings->setValue("RightPanePath", rightPane->pathLineEdit->text());
    settings->setValue("RightPaneFileListHeader", rightPane->treeView->header()->saveState());
    settings->setValue("RightPaneViewMode", rightPane->stackedWidget->currentIndex());
    settings->setValue("ShowHidden", hiddenAction->isChecked());
}

void MainWindow::restoreState()
{
    restoreGeometry(settings->value("Geometry").toByteArray());
    toolBar->setVisible(settings->value("ShowToolBar", QVariant(true)).toBool());
    statusBar()->setVisible(settings->value("ShowStatusBar", QVariant(false)).toBool());
    splitter->restoreState(settings->value("MainSplitterSizes").toByteArray());
    setActivePane(settings->value("LeftPaneActive", 1).toBool() ? leftPane : rightPane);
    leftPane->treeView->header()->restoreState(settings->value("LeftPaneFileListHeader").toByteArray());
    leftPane->moveTo(settings->value("LeftPanePath", "").toString());
    leftPane->stackedWidget->setCurrentIndex(settings->value("LeftPaneViewMode", 0).toInt());
    rightPane->treeView->header()->restoreState(settings->value("RightPaneFileListHeader").toByteArray());
    rightPane->moveTo(settings->value("RightPanePath", "").toString());
    rightPane->stackedWidget->setCurrentIndex(settings->value("RightPaneViewMode", 0).toInt());
    hiddenAction->setChecked(settings->value("ShowHidden", false).toBool());
    toggleHidden();
}

void MainWindow::updateViewActions()
{
    switch (activePane->stackedWidget->currentIndex())
    {
    case Pane::TreeViewMode:
        detailViewAction->setChecked(true);
        break;
    case Pane::ListViewMode:
        iconViewAction->setChecked(true);
        break;
    }
}

void MainWindow::showAboutBox()
{
    QMessageBox::about(this, tr("О программе"),
                       tr("<h2>File Manager</h2>"
                          "группа 053502 БГУИР ФКСиС ИиТП<br>"
                          "2021 Володьков Савелий<br>"));
}

void MainWindow::showPreferences()
{
    PreferencesDialog preferences(this);
    preferences.exec();
}

void MainWindow::showProperties()
{
    Properties properties(this);
    properties.exec();
}


//-----------------------

bool FileSystemModelFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    QModelIndex index0 = sourceModel()->index(sourceRow, 0, sourceParent);
    QFileSystemModel* fileSystemModel = qobject_cast<QFileSystemModel*>(sourceModel());
    if (fileSystemModel->isDir(index0) && fileSystemModel->fileName(index0).compare("..") != 0)
        return true;
    else
        return false;
}
