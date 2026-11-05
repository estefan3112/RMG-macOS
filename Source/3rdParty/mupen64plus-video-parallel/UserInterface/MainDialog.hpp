/*
 * Rosalie's Mupen GUI - https://github.com/Rosalie241/RMG
 *  Copyright (C) 2020 Rosalie Wanders <rosalie@mailbox.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP

#include <QDialog>
#include <QAbstractButton>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>

#include "ui_MainDialog.h"

namespace UserInterface
{
class MainDialog : public QDialog, private Ui::MainDialog
{
Q_OBJECT
public:
    MainDialog(QWidget *parent);
    ~MainDialog(void);

private:
    // shader tab, created in code
    QComboBox* shaderPresetComboBox = nullptr;
    QLineEdit* shaderCustomLineEdit = nullptr;
    QPushButton* shaderBrowseButton = nullptr;
    QCheckBox* shaderFullWindowCheckBox = nullptr;

    void setupShaderTab(void);
    void setShaderPreset(const QString& preset);
    QString getShaderPreset(void);
    QString getBundledShaderDirectory(void);

private slots:
    void on_buttonBox_clicked(QAbstractButton* button);
    void shaderPresetChanged(int index);
    void browseShaderPreset(void);
};
}

#endif // MAINWINDOW_HPP