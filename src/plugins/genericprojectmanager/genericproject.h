/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#pragma once

#include <projectexplorer/headerpath.h>
#include <projectexplorer/project.h>
#include <utils/fileutils.h>

namespace CppTools { class CppProjectUpdater; }
namespace Utils { class FileSystemWatcher; }

namespace GenericProjectManager {
namespace Internal {

class GenericProject : public ProjectExplorer::Project
{
    Q_OBJECT

public:
    explicit GenericProject(const Utils::FilePath &filename);
    ~GenericProject() override;

    bool addFiles(const QStringList &filePaths);
    bool removeFiles(const QStringList &filePaths);
    bool setFiles(const QStringList &filePaths);
    bool renameFile(const QString &filePath, const QString &newFilePath);

    Utils::FilePath filesFilePath() const { return Utils::FilePath::fromString(m_filesFileName); }

    enum RefreshOptions {
        Files         = 0x01,
        Configuration = 0x02,
        Everything    = Files | Configuration
    };

    void refresh(RefreshOptions options);

protected:
    RestoreResult fromMap(const QVariantMap &map, QString *errorMessage) override;

private:
    ProjectExplorer::DeploymentKnowledge deploymentKnowledge() const override;

    bool saveRawFileList(const QStringList &rawFileList);
    bool saveRawList(const QStringList &rawList, const QString &fileName);
    void parseProject(RefreshOptions options);
    QStringList processEntries(const QStringList &paths,
                               QHash<QString, QString> *map = nullptr) const;

    Utils::FilePath findCommonSourceRoot();
    void refreshCppCodeModel();
    void updateDeploymentData();

    QString m_filesFileName;
    QString m_includesFileName;
    QString m_configFileName;
    QString m_cxxflagsFileName;
    QString m_cflagsFileName;
    QStringList m_rawFileList;
    QStringList m_files;
    QHash<QString, QString> m_rawListEntries;
    QStringList m_rawProjectIncludePaths;
    ProjectExplorer::HeaderPaths m_projectIncludePaths;
    QStringList m_cxxflags;
    QStringList m_cflags;

    CppTools::CppProjectUpdater *m_cppCodeModelUpdater = nullptr;

    Utils::FileSystemWatcher * const m_deployFileWatcher = nullptr;
};

} // namespace Internal
} // namespace GenericProjectManager
