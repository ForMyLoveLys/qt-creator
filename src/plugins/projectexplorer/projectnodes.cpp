/***************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2008 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact:  Qt Software Information (qt-info@nokia.com)
**
**
** Non-Open Source Usage
**
** Licensees may use this file in accordance with the Qt Beta Version
** License Agreement, Agreement version 2.2 provided with the Software or,
** alternatively, in accordance with the terms contained in a written
** agreement between you and Nokia.
**
** GNU General Public License Usage
**
** Alternatively, this file may be used under the terms of the GNU General
** Public License versions 2.0 or 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the packaging
** of this file.  Please review the following information to ensure GNU
** General Public Licensing requirements will be met:
**
** http://www.fsf.org/licensing/licenses/info/GPLv2.html and
** http://www.gnu.org/copyleft/gpl.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt GPL Exception
** version 1.2, included in the file GPL_EXCEPTION.txt in this package.
**
***************************************************************************/

#include "nodesvisitor.h"
#include "projectnodes.h"
#include "projectexplorerconstants.h"

#include <coreplugin/mimedatabase.h>

#include <QtCore/QFileInfo>
#include <QtGui/QApplication>
#include <QtGui/QIcon>
#include <QtGui/QStyle>

using namespace ProjectExplorer;

/*!
  \class FileNode

  Base class of all nodes in the node hierarchy.

  \see FileNode
  \see FolderNode
  \see ProjectNode
*/
Node::Node(NodeType nodeType,
           const QString &filePath)
        : QObject(),
          m_nodeType(nodeType),
          m_projectNode(0),
          m_folderNode(0),
          m_path(filePath)
{
}

NodeType Node::nodeType() const
{
    return m_nodeType;
}

/*!
  Project that owns & manages the node. It's the first project in list of ancestors.
  */
ProjectNode *Node::projectNode() const
{
    return m_projectNode;
}

/*!
  Parent in node hierarchy.
  */
FolderNode *Node::parentFolderNode() const
{
    return m_folderNode;
}

/*!
  Path of file or folder in the filesystem the node represents.
  */
QString Node::path() const
{
    return m_path;
}

void Node::setNodeType(NodeType type)
{
    m_nodeType = type;
}

void Node::setProjectNode(ProjectNode *project)
{
    m_projectNode = project;
}

void Node::setParentFolderNode(FolderNode *parentFolder)
{
    m_folderNode = parentFolder;
}

void Node::setPath(const QString &path)
{
    m_path = path;
}

/*!
  \class FileNode

  In-memory presentation of a file. All FileNode's are leaf nodes.

  \see FolderNode
  \see ProjectNode
*/

FileNode::FileNode(const QString &filePath,
                   const FileType fileType,
                   bool generated)
        : Node(FileNodeType, filePath),
          m_fileType(fileType),
          m_generated(generated)
{
}

FileType FileNode::fileType() const
{
    return m_fileType;
}

/*!
  Returns true if the file is automatically generated by a compile step.
  */
bool FileNode::isGenerated() const
{
    return m_generated;
}

/*!
  \class FolderNode

  In-memory presentation of a folder. Note that the node itself + all children (files and folders) are "managed" by the owning project.

  \see FileNode
  \see ProjectNode
*/
FolderNode::FolderNode(const QString &folderPath)
        : Node(FolderNodeType, folderPath),
          m_folderName(folderPath)
{
    m_icon = QApplication::style()->standardIcon(QStyle::SP_DirIcon);
}

FolderNode::~FolderNode()
{
    qDeleteAll(m_subFolderNodes);
    qDeleteAll(m_fileNodes);
}

/*
  The display name that should be used in a view.


  \see setFolderName()
 */
QString FolderNode::name() const
{
    return m_folderName;
}

/*
  The icon that should be used in a view. Default is the directory icon (QStyle::S_PDirIcon).
  \see setIcon()
 */
QIcon FolderNode::icon() const
{
    return m_icon;
}

QList<FileNode*> FolderNode::fileNodes() const
{
    return m_fileNodes;
}

QList<FolderNode*> FolderNode::subFolderNodes() const
{
    return m_subFolderNodes;
}

void FolderNode::accept(NodesVisitor *visitor)
{
    visitor->visitFolderNode(this);
    foreach (FolderNode *subFolder, m_subFolderNodes)
        subFolder->accept(visitor);
}

void FolderNode::setFolderName(const QString &name)
{
    m_folderName = name;
}

void FolderNode::setIcon(const QIcon &icon)
{
    m_icon = icon;
}

/*!
  \class ProjectNode

  In-memory presentation of a Project.
  A concrete subclass must implement the "persistent" stuff

  \see FileNode
  \see FolderNode
*/

/*
  Creates uninitialized ProjectNode object.
  */
ProjectNode::ProjectNode(const QString &projectFilePath)
        : FolderNode(projectFilePath)
{
    setNodeType(ProjectNodeType);
    // project node "manages" itself
    setProjectNode(this);
    setFolderName(QFileInfo(m_folderName).fileName());
}

QList<ProjectNode*> ProjectNode::subProjectNodes() const
{
    return m_subProjectNodes;
}

/*!
  \function bool ProjectNode::addSubProjects(const QStringList &)
  */

/*!
  \function bool ProjectNode::removeSubProjects(const QStringList &)
  */

/*!
  \function bool ProjectNode::addFiles(const FileType, const QStringList &, QStringList *)
  */

/*!
  \function bool ProjectNode::removeFiles(const FileType, const QStringList &, QStringList *)
  */

/*!
  \function bool ProjectNode::renameFile(const FileType, const QString &, const QString &)
  */

QList<NodesWatcher*> ProjectNode::watchers() const
{
    return m_watchers;
}

/*
   Registers a watcher for the current project & all sub projects
   It does not take ownership of the watcher.
   */
void ProjectNode::registerWatcher(NodesWatcher *watcher)
{
    if (!watcher)
        return;
    connect(watcher, SIGNAL(destroyed(QObject *)),
            this, SLOT(watcherDestroyed(QObject *)));
    m_watchers.append(watcher);
    foreach (ProjectNode *subProject, m_subProjectNodes)
        subProject->registerWatcher(watcher);
}

/*
  Removes a watcher for the current project & all sub projects.
*/
void ProjectNode::unregisterWatcher(NodesWatcher *watcher)
{
    if (!watcher)
        return;
    m_watchers.removeOne(watcher);
    foreach (ProjectNode *subProject, m_subProjectNodes)
        subProject->unregisterWatcher(watcher);
}

void ProjectNode::accept(NodesVisitor *visitor)
{
    visitor->visitProjectNode(this);

    foreach (FolderNode *folder, m_subFolderNodes)
        folder->accept(visitor);
}

/*!
  Adds project nodes to the hierarchy and emits the corresponding signals.
  */
void ProjectNode::addProjectNodes(const QList<ProjectNode*> &subProjects)
{
    if (!subProjects.isEmpty()) {
        QList<FolderNode*> folderNodes;
        foreach (ProjectNode *projectNode, subProjects)
            folderNodes << projectNode;

        foreach (NodesWatcher *watcher, m_watchers)
            emit watcher->foldersAboutToBeAdded(this, folderNodes);

        foreach (ProjectNode *project, subProjects) {
            Q_ASSERT_X(!project->parentFolderNode(), "addProjectNodes",
                       "Project node has already a parent");
            project->setParentFolderNode(this);
            foreach (NodesWatcher *watcher, m_watchers)
                project->registerWatcher(watcher);
            m_subFolderNodes.append(project);
            m_subProjectNodes.append(project);
        }
        qSort(m_subFolderNodes.begin(), m_subFolderNodes.end(),
              sortNodesByPath);
        qSort(m_subProjectNodes.begin(), m_subProjectNodes.end(),
              sortNodesByPath);

        foreach (NodesWatcher *watcher, m_watchers)
            emit watcher->foldersAdded();
    }
}

/*!
  Remove project nodes from the hierarchy and emits the corresponding signals.
  All objects in the argument list are deleted.
  */
void ProjectNode::removeProjectNodes(const QList<ProjectNode*> &subProjects)
{
    if (!subProjects.isEmpty()) {
        QList<FolderNode*> toRemove;
        foreach (ProjectNode *projectNode, subProjects)
            toRemove << projectNode;
        qSort(toRemove.begin(), toRemove.end(), sortNodesByPath);

        foreach (NodesWatcher *watcher, m_watchers)
            emit watcher->foldersAboutToBeRemoved(this, toRemove);

        QList<FolderNode*>::const_iterator toRemoveIter = toRemove.constBegin();
        QList<FolderNode*>::iterator folderIter = m_subFolderNodes.begin();
        QList<ProjectNode*>::iterator projectIter = m_subProjectNodes.begin();
        for (; toRemoveIter != toRemove.constEnd(); ++toRemoveIter) {
            while ((*projectIter)->path() != (*toRemoveIter)->path()) {
                ++projectIter;
                Q_ASSERT_X(projectIter != m_subProjectNodes.end(), "removeProjectNodes",
                           "Project to remove is not part of specified folder!");
            }
            while ((*folderIter)->path() != (*toRemoveIter)->path()) {
                ++folderIter;
                Q_ASSERT_X(folderIter != m_subFolderNodes.end(), "removeProjectNodes",
                           "Project to remove is not part of specified folder!");
            }
            delete *projectIter;
            projectIter = m_subProjectNodes.erase(projectIter);
            folderIter = m_subFolderNodes.erase(folderIter);
        }

        foreach (NodesWatcher *watcher, m_watchers)
            emit watcher->foldersRemoved();
    }
}

/*!
  Adds folder nodes to the hierarchy and emits the corresponding signals.
  */
void ProjectNode::addFolderNodes(const QList<FolderNode*> &subFolders, FolderNode *parentFolder)
{
    Q_ASSERT(parentFolder);

    if (!subFolders.isEmpty()) {
        const bool emitSignals = (parentFolder->projectNode() == this);

        if (emitSignals)
            foreach (NodesWatcher *watcher, m_watchers)
                watcher->foldersAboutToBeAdded(parentFolder, subFolders);

        foreach (FolderNode *folder, subFolders) {
            Q_ASSERT_X(!folder->parentFolderNode(), "addFolderNodes",
                       "Project node has already a parent folder");
            folder->setParentFolderNode(parentFolder);
            folder->setProjectNode(this);
            parentFolder->m_subFolderNodes.append(folder);

            // project nodes have to be added via addProjectNodes
            Q_ASSERT_X(folder->nodeType() != ProjectNodeType, "addFolderNodes",
                       "project nodes have to be added via addProjectNodes");
        }
        qSort(parentFolder->m_subFolderNodes.begin(), parentFolder->m_subFolderNodes.end(),
              sortNodesByPath);

        if (emitSignals)
            foreach (NodesWatcher *watcher, m_watchers)
                emit watcher->foldersAdded();
    }
}

/*!
  Remove file nodes from the hierarchy and emits the corresponding signals.
  All objects in the subFolders list are deleted.
  */
void ProjectNode::removeFolderNodes(const QList<FolderNode*> &subFolders,
                                   FolderNode *parentFolder)
{
    Q_ASSERT(parentFolder);

    if (!subFolders.isEmpty()) {
        const bool emitSignals = (parentFolder->projectNode() == this);

        QList<FolderNode*> toRemove = subFolders;
        qSort(toRemove.begin(), toRemove.end(), sortNodesByPath);

        if (emitSignals)
            foreach (NodesWatcher *watcher, m_watchers)
                emit watcher->foldersAboutToBeRemoved(parentFolder, toRemove);

        QList<FolderNode*>::const_iterator toRemoveIter = toRemove.constBegin();
        QList<FolderNode*>::iterator folderIter = parentFolder->m_subFolderNodes.begin();
        for (; toRemoveIter != toRemove.constEnd(); ++toRemoveIter) {
            Q_ASSERT_X(((*toRemoveIter)->nodeType() != ProjectNodeType), "removeFolderNodes",
                           "project nodes have to be removed via removeProjectNodes");
            while ((*folderIter)->path() != (*toRemoveIter)->path()) {
                ++folderIter;
                Q_ASSERT_X(folderIter != parentFolder->m_subFolderNodes.end(), "removeFileNodes",
                           "Folder to remove is not part of specified folder!");
            }
            delete *folderIter;
            folderIter = parentFolder->m_subFolderNodes.erase(folderIter);
        }

        if (emitSignals)
            foreach (NodesWatcher *watcher, m_watchers)
                emit watcher->foldersRemoved();
    }
}

/*!
  Adds file nodes to the internal list and emits the corresponding signals.
  This method should be called within an implementation of the public method addFiles.
  */
void ProjectNode::addFileNodes(const QList<FileNode*> &files, FolderNode *folder)
{
    Q_ASSERT(folder);

    if (!files.isEmpty()) {
        const bool emitSignals = (folder->projectNode() == this);

        if (emitSignals)
            foreach (NodesWatcher *watcher, m_watchers)
                emit watcher->filesAboutToBeAdded(folder, files);

        foreach (FileNode *file, files) {
            Q_ASSERT_X(!file->parentFolderNode(), "addFileNodes",
                       "File node has already a parent folder");

            file->setParentFolderNode(folder);
            file->setProjectNode(this);
            folder->m_fileNodes.append(file);
        }
        qSort(folder->m_fileNodes.begin(), folder->m_fileNodes.end(), sortNodesByPath);

        if (emitSignals)
            foreach (NodesWatcher *watcher, m_watchers)
                emit watcher->filesAdded();
    }
}

/*!
  Remove file nodes from the internal list and emits the corresponding signals.
  All objects in the argument list are deleted.
  This method should be called within an implementation of the public method removeFiles.
  */
void ProjectNode::removeFileNodes(const QList<FileNode*> &files, FolderNode *folder)
{
    Q_ASSERT(folder);

    if (!files.isEmpty()) {
        const bool emitSignals = (folder->projectNode() == this);

        QList<FileNode*> toRemove = files;
        qSort(toRemove.begin(), toRemove.end(), sortNodesByPath);

        if (emitSignals)
            foreach (NodesWatcher *watcher, m_watchers)
                emit watcher->filesAboutToBeRemoved(folder, toRemove);

        QList<FileNode*>::const_iterator toRemoveIter = toRemove.constBegin();
        QList<FileNode*>::iterator filesIter = folder->m_fileNodes.begin();
        for (; toRemoveIter != toRemove.constEnd(); ++toRemoveIter) {
            while ((*filesIter)->path() != (*toRemoveIter)->path()) {
                ++filesIter;
                Q_ASSERT_X(filesIter != folder->m_fileNodes.end(), "removeFileNodes",
                           "File to remove is not part of specified folder!");
            }
            delete *filesIter;
            filesIter = folder->m_fileNodes.erase(filesIter);
        }

        if (emitSignals)
            foreach (NodesWatcher *watcher, m_watchers)
                emit watcher->filesRemoved();
    }
}

void ProjectNode::watcherDestroyed(QObject *watcher)
{
    // cannot use qobject_cast here
    unregisterWatcher(static_cast<NodesWatcher*>(watcher));
}

/*!
  Sort pointers to FileNodes
  */
bool ProjectNode::sortNodesByPath(Node *f1, Node *f2) {
    return f1->path() < f2->path();
}

/*!
  \class SessionNode
*/

SessionNode::SessionNode(const QString &sessionPath, QObject *parentObject)
        : FolderNode(sessionPath)
{
    setParent(parentObject);
    setNodeType(SessionNodeType);
}

QList<NodesWatcher*> SessionNode::watchers() const
{
    return m_watchers;
}

/*
   Registers a watcher for the complete session tree.
   It does not take ownership of the watcher.
*/
void SessionNode::registerWatcher(NodesWatcher *watcher)
{
    if (!watcher)
        return;
    connect(watcher, SIGNAL(destroyed(QObject*)),
            this, SLOT(watcherDestroyed(QObject*)));
    m_watchers.append(watcher);
    foreach (ProjectNode *project, m_projectNodes)
        project->registerWatcher(watcher);
}

/*
    Removes a watcher from the complete session tree
*/
void SessionNode::unregisterWatcher(NodesWatcher *watcher)
{
    if (!watcher)
        return;
    m_watchers.removeOne(watcher);
    foreach (ProjectNode *project, m_projectNodes)
        project->unregisterWatcher(watcher);
}

void SessionNode::accept(NodesVisitor *visitor)
{
    visitor->visitSessionNode(this);
    foreach (ProjectNode *project, m_projectNodes)
        project->accept(visitor);
}

QList<ProjectNode*> SessionNode::projectNodes() const
{
    return m_projectNodes;
}

void SessionNode::addProjectNodes(const QList<ProjectNode*> &projectNodes)
{
    if (!projectNodes.isEmpty()) {
        QList<FolderNode*> folderNodes;
        foreach (ProjectNode *projectNode, projectNodes)
            folderNodes << projectNode;

        foreach (NodesWatcher *watcher, m_watchers)
            emit watcher->foldersAboutToBeAdded(this, folderNodes);

        foreach (ProjectNode *project, projectNodes) {
            Q_ASSERT_X(!project->parentFolderNode(), "addProjectNodes",
                       "Project node has already a parent folder");
            project->setParentFolderNode(this);
            foreach (NodesWatcher *watcher, m_watchers)
                project->registerWatcher(watcher);
            m_subFolderNodes.append(project);
            m_projectNodes.append(project);
        }

        foreach (NodesWatcher *watcher, m_watchers)
            emit watcher->foldersAdded();
   }
}

void SessionNode::removeProjectNodes(const QList<ProjectNode*> &projectNodes)
{
    if (!projectNodes.isEmpty()) {
        QList<FolderNode*> toRemove;
        foreach (ProjectNode *projectNode, projectNodes)
            toRemove << projectNode;

        foreach (NodesWatcher *watcher, m_watchers)
            emit watcher->foldersAboutToBeRemoved(this, toRemove);

        QList<FolderNode*>::const_iterator toRemoveIter = toRemove.constBegin();
        QList<FolderNode*>::iterator folderIter = m_subFolderNodes.begin();
        QList<ProjectNode*>::iterator projectIter = m_projectNodes.begin();
        for (; toRemoveIter != toRemove.constEnd(); ++toRemoveIter) {
            while ((*projectIter)->path() != (*toRemoveIter)->path()) {
                ++projectIter;
                Q_ASSERT_X(projectIter != m_projectNodes.end(), "removeProjectNodes",
                           "Project to remove is not part of specified folder!");
            }
            while ((*folderIter)->path() != (*toRemoveIter)->path()) {
                ++folderIter;
                Q_ASSERT_X(folderIter != m_subFolderNodes.end(), "removeProjectNodes",
                           "Project to remove is not part of specified folder!");
            }
            projectIter = m_projectNodes.erase(projectIter);
            folderIter = m_subFolderNodes.erase(folderIter);
        }

        foreach (NodesWatcher *watcher, m_watchers)
            emit watcher->foldersRemoved();
    }
}

void SessionNode::watcherDestroyed(QObject *watcher)
{
    // cannot use qobject_cast here
    unregisterWatcher(static_cast<NodesWatcher*>(watcher));
}

/*!
  \class NodesWatcher

  NodesWatcher let you keep track of changes in the tree.

  Add a watcher by calling ProjectNode::registerWatcher() or
  SessionNode::registerWatcher(). Whenever the tree underneath the
  ProectNode or SessionNode changes (e.g. nodes are added/removed),
  the corresponding signals of the NodesWatcher are emitted.
  Watchers can be removed from the complete tree or a subtree
  by calling ProjectNode::unregisterWatcher and
  SessionNode::unregisterWatcher().

  The NodesWatcher is similar to the Observer in the
  well-known Observer pattern (Booch et al).
*/

NodesWatcher::NodesWatcher(QObject *parent)
        : QObject(parent)
{
}

// TODO Maybe put this in core, so that all can benefit
FileType typeForFileName(const Core::MimeDatabase *db, const QFileInfo &file)
{
    const Core::MimeType mt = db->findByFile(file);
    if (!mt)
        return UnknownFileType;

    const QString typeName = mt.type();
    if (typeName == QLatin1String(Constants::CPP_SOURCE_MIMETYPE)
        || typeName == QLatin1String(Constants::C_SOURCE_MIMETYPE))
        return SourceType;
    if (typeName == QLatin1String(Constants::CPP_HEADER_MIMETYPE)
        || typeName == QLatin1String(Constants::C_HEADER_MIMETYPE))
        return HeaderType;
    if (typeName == QLatin1String(Constants::RESOURCE_MIMETYPE))
        return ResourceType;
    if (typeName == QLatin1String(Constants::FORM_MIMETYPE))
        return FormType;
    return UnknownFileType;
}
