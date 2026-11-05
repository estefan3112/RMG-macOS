/*
 * Rosalie's Mupen GUI - https://github.com/Rosalie241/RMG
 *  Copyright (C) 2020 Rosalie Wanders <rosalie@mailbox.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#include "MainDialog.hpp"
#include "m64p_types.h"
#include "m64p_config.h"
#include "../gfx_m64p.h"

#include <QRegularExpression>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>

// bundled presets, relative to the Shaders folder in RMG's data directory
static const struct
{
    const char* name;
    const char* preset;
} l_BundledShaderPresets[] =
{
    { "crt-easymode",  "crt/crt-easymode.slangp" },
    { "crt-cgwg-fast", "crt/crt-cgwg-fast.slangp" },
    { "koko-aio",      "bezel/koko-aio/koko-aio-ng.slangp" },
};

using namespace UserInterface;

MainDialog::MainDialog(QWidget* parent) : QDialog(parent)
{
    this->setupUi(this);

    // set screen size combobox
    int width  = ConfigGetParamInt(configVideoParallel, KEY_SCREEN_WIDTH);
    int height = ConfigGetParamInt(configVideoParallel, KEY_SCREEN_HEIGHT);

    // taken from GLideN64's GLideNUI
    QRegularExpression windowedRegExp("([3-9][0-9]{2}|[1-7][0-9]{3}) ?x ?([2-9][0-9]{2}|[1-3][0-9]{3})");
    QValidator *windowedValidator = new QRegularExpressionValidator(windowedRegExp, this);
    this->screenSizeComboBox->setValidator(windowedValidator);

    QString resolutionString = QString::number(width) + " x " + QString::number(height);

    // add resolution if it doesnt exist
    if (this->screenSizeComboBox->findText(resolutionString) == -1)
    {
        this->screenSizeComboBox->addItem(resolutionString);
    }
    // set resolution as current item
    this->screenSizeComboBox->setCurrentText(resolutionString);

    // set upscaling combobox
    int upscaleAmount = ConfigGetParamInt(configVideoParallel, KEY_UPSCALING);
    int comboBoxIndex = 0;
    switch (upscaleAmount)
    {
    case 1:
    case 2:
        comboBoxIndex = upscaleAmount - 1;
        break;
    case 4:
        comboBoxIndex = 2;
        break;
    case 8:
        comboBoxIndex = 3;
        break;
    default:
        this->resolutionUpscaleComboBox->addItem(QString::number(upscaleAmount) + "x");
        comboBoxIndex = 4;
        break;
    }
    this->resolutionUpscaleComboBox->setCurrentIndex(comboBoxIndex);

    this->deinterlacerComboBox->setCurrentIndex(ConfigGetParamBool(configVideoParallel, KEY_DEINTERLACE) ? 1 : 0);
    this->downscalerComboBox->setCurrentIndex(ConfigGetParamInt(configVideoParallel, KEY_DOWNSCALE));
    this->verticalStretchSpinBox->setValue(ConfigGetParamInt(configVideoParallel, KEY_VERTICAL_STRETCH));
    
    this->overscanGroupBox->setChecked(ConfigGetParamBool(configVideoParallel, KEY_OVERSCANCROPENABLE));
    this->overscanLeftSpinBox->setValue(ConfigGetParamInt(configVideoParallel, KEY_OVERSCANCROPLEFT));
    this->overscanRightSpinBox->setValue(ConfigGetParamInt(configVideoParallel, KEY_OVERSCANCROPRIGHT));
    this->overscanTopSpinBox->setValue(ConfigGetParamInt(configVideoParallel, KEY_OVERSCANCROPTOP));
    this->overscanBottomSpinBox->setValue(ConfigGetParamInt(configVideoParallel, KEY_OVERSCANCROPBOTTOM));

    this->superSampledDitherCheckBox->setChecked(ConfigGetParamBool(configVideoParallel, KEY_SSDITHER));
    this->viAaCheckBox->setChecked(ConfigGetParamBool(configVideoParallel, KEY_AA));
    this->viDivotFilterCheckBox->setChecked(ConfigGetParamBool(configVideoParallel, KEY_DIVOT));
    this->viDeDitheringCheckBox->setChecked(ConfigGetParamBool(configVideoParallel, KEY_VIDITHER));
    this->nativeTextureLodCheckBox->setChecked(ConfigGetParamBool(configVideoParallel, KEY_NATIVETEXTLOD));
    this->forceWidescreenCheckBox->setChecked(ConfigGetParamBool(configVideoParallel, KEY_WIDESCREEN));

    this->supersampledRdramReadsCheckBox->setChecked(ConfigGetParamBool(configVideoParallel, KEY_SSREADBACKS));
    this->viBilinearFilteringCheckBox->setChecked(ConfigGetParamBool(configVideoParallel, KEY_VIBILERP));
    this->viGammaDitherCheckBox->setChecked(ConfigGetParamBool(configVideoParallel, KEY_GAMMADITHER));
    this->nativeTextRectsCheckBox->setChecked(ConfigGetParamBool(configVideoParallel, KEY_NATIVETEXTRECT));
    this->verticalSyncCheckBox->setChecked(ConfigGetParamBool(configVideoParallel, KEY_VSYNC));

    this->setupShaderTab();
    const char* shaderPreset = ConfigGetParamString(configVideoParallel, KEY_SHADERPRESET);
    this->setShaderPreset(shaderPreset != nullptr ? QString::fromUtf8(shaderPreset) : QString());
    this->shaderFullWindowCheckBox->setChecked(ConfigGetParamBool(configVideoParallel, KEY_SHADERFULLWINDOW));
}

void MainDialog::setupShaderTab(void)
{
    QWidget* tab = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(tab);
    QFormLayout* formLayout = new QFormLayout();

    this->shaderPresetComboBox = new QComboBox(tab);
    this->shaderPresetComboBox->addItem("Off", QString());
    for (const auto& bundledPreset : l_BundledShaderPresets)
    {
        this->shaderPresetComboBox->addItem(bundledPreset.name, QString(bundledPreset.preset));
    }
    this->shaderPresetComboBox->addItem("Custom...", QString("custom"));
    formLayout->addRow("Shader preset:", this->shaderPresetComboBox);

    QHBoxLayout* customLayout = new QHBoxLayout();
    this->shaderCustomLineEdit = new QLineEdit(tab);
    this->shaderCustomLineEdit->setPlaceholderText("Path to a .slangp preset");
    this->shaderBrowseButton = new QPushButton("Browse...", tab);
    customLayout->addWidget(this->shaderCustomLineEdit);
    customLayout->addWidget(this->shaderBrowseButton);
    formLayout->addRow("Custom preset:", customLayout);
    layout->addLayout(formLayout);

    this->shaderFullWindowCheckBox = new QCheckBox("Use the whole window (for bezels and backgrounds, e.g. koko-aio)", tab);
    layout->addWidget(this->shaderFullWindowCheckBox);

    QLabel* hintLabel = new QLabel(tab);
    hintLabel->setWordWrap(true);
    hintLabel->setText("Shader presets are RetroArch slang shaders (.slangp), "
                       "applied to the final image. CRT shaders expect the game's native "
                       "resolution: when upscaling, set the downscaler so it cancels the "
                       "upscaling out (e.g. 4x upscaling with 1/4 downscaling).\n\n"
                       "With Custom, any preset of the full slang-shaders collection can be "
                       "used, including the koko-aio variants in the bundled "
                       "bezel/koko-aio folder. Shaders need librashader.\n\n"
                       "Use the whole window only with shaders that keep the aspect ratio "
                       "themselves (like koko-aio with bezels), others get stretched.");
    layout->addWidget(hintLabel);
    layout->addStretch();

    this->tabWidget->addTab(tab, "Shader");

    connect(this->shaderPresetComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainDialog::shaderPresetChanged);
    connect(this->shaderBrowseButton, &QPushButton::clicked,
            this, &MainDialog::browseShaderPreset);
}

void MainDialog::setShaderPreset(const QString& preset)
{
    int index = 0;
    if (!preset.isEmpty())
    {
        index = this->shaderPresetComboBox->findData(preset);
        if (index == -1)
        { // not a bundled preset
            index = this->shaderPresetComboBox->count() - 1;
            this->shaderCustomLineEdit->setText(preset);
        }
    }
    this->shaderPresetComboBox->setCurrentIndex(index);
    this->shaderPresetChanged(index);
}

QString MainDialog::getShaderPreset(void)
{
    int index = this->shaderPresetComboBox->currentIndex();
    if (index == this->shaderPresetComboBox->count() - 1)
    {
        return this->shaderCustomLineEdit->text().trimmed();
    }
    return this->shaderPresetComboBox->itemData(index).toString();
}

QString MainDialog::getBundledShaderDirectory(void)
{
    if (ConfigGetSharedDataFilepath != nullptr)
    {
        const char* readme = ConfigGetSharedDataFilepath("Shaders/README.md");
        if (readme != nullptr)
        {
            return QFileInfo(QString::fromUtf8(readme)).absolutePath();
        }
    }
    return QDir::homePath();
}

void MainDialog::shaderPresetChanged(int index)
{
    bool custom = index == this->shaderPresetComboBox->count() - 1;
    this->shaderCustomLineEdit->setEnabled(custom);
    this->shaderBrowseButton->setEnabled(custom);
}

void MainDialog::browseShaderPreset(void)
{
    QString startDirectory = this->getBundledShaderDirectory();
    QString currentPreset = this->shaderCustomLineEdit->text().trimmed();
    if (!currentPreset.isEmpty() && QFileInfo(currentPreset).isAbsolute())
    {
        startDirectory = QFileInfo(currentPreset).absolutePath();
    }

    QString preset = QFileDialog::getOpenFileName(this, "Choose shader preset", startDirectory,
                                                  "Shader presets (*.slangp)");
    if (!preset.isEmpty())
    {
        this->shaderCustomLineEdit->setText(preset);
    }
}

MainDialog::~MainDialog()
{
}

void MainDialog::on_buttonBox_clicked(QAbstractButton* button)
{
    QPushButton *pushButton = (QPushButton *)button;
    QPushButton *okButton = this->buttonBox->button(QDialogButtonBox::Ok);
    QPushButton *defaultButton = this->buttonBox->button(QDialogButtonBox::RestoreDefaults);

    if (pushButton == okButton)
    {
        // screen size
        int width  = this->screenSizeComboBox->currentText().split(" x ").at(0).toInt();
        int height = this->screenSizeComboBox->currentText().split(" x ").at(1).toInt();
        ConfigSetParameter(configVideoParallel, KEY_SCREEN_WIDTH, M64TYPE_INT, &width);
        ConfigSetParameter(configVideoParallel, KEY_SCREEN_HEIGHT, M64TYPE_INT, &height);

        // upscaling
        int upscalingIndex = this->resolutionUpscaleComboBox->currentIndex();
        int upscalingValue = 0;
        if (upscalingIndex == 0)
        {
            upscalingValue = 1;
        }
        else
        {
            upscalingValue = this->resolutionUpscaleComboBox->currentText().split("x").at(0).toInt();
        }
        ConfigSetParameter(configVideoParallel, KEY_UPSCALING, M64TYPE_INT, &upscalingValue);
    
        // deinterlacer
        int deinterlacerValue = this->deinterlacerComboBox->currentIndex();
        ConfigSetParameter(configVideoParallel, KEY_DEINTERLACE, M64TYPE_BOOL, &deinterlacerValue);

        // downscaler
        int downscalingValue = this->downscalerComboBox->currentIndex();
        ConfigSetParameter(configVideoParallel, KEY_DOWNSCALE, M64TYPE_INT, &downscalingValue);

        // cropOverscan
        int overscanEnabled = this->overscanGroupBox->isChecked();
        int overscanLeft = this->overscanLeftSpinBox->value();
        int overscanRight = this->overscanRightSpinBox->value();
        int overscanTop = this->overscanTopSpinBox->value();
        int overscanBottom = this->overscanBottomSpinBox->value();
        ConfigSetParameter(configVideoParallel, KEY_OVERSCANCROPENABLE, M64TYPE_BOOL, &overscanEnabled);
        ConfigSetParameter(configVideoParallel, KEY_OVERSCANCROPLEFT, M64TYPE_INT, &overscanLeft);
        ConfigSetParameter(configVideoParallel, KEY_OVERSCANCROPRIGHT, M64TYPE_INT, &overscanRight);
        ConfigSetParameter(configVideoParallel, KEY_OVERSCANCROPTOP, M64TYPE_INT, &overscanTop);
        ConfigSetParameter(configVideoParallel, KEY_OVERSCANCROPBOTTOM, M64TYPE_INT, &overscanBottom);

        // verticalStretch
        int verticalStretchValue = this->verticalStretchSpinBox->value();
        ConfigSetParameter(configVideoParallel, KEY_VERTICAL_STRETCH, M64TYPE_INT, &verticalStretchValue);

        // checkboxes
        int superSampledDitherValue = this->superSampledDitherCheckBox->isChecked() ? 1 : 0;
        int viAaValue = this->viAaCheckBox->isChecked() ? 1 : 0;
        int viDivotFilterValue = this->viDivotFilterCheckBox->isChecked() ? 1 : 0; 
        int viDeDitheringValue = this->viDeDitheringCheckBox->isChecked() ? 1 : 0;
        int nativeTextureLodValue = this->nativeTextureLodCheckBox->isChecked() ? 1 : 0; 
        int forceWidescreenValue = this->forceWidescreenCheckBox->isChecked() ? 1 : 0; 
        int supersampledRdramReadsValue = this->supersampledRdramReadsCheckBox->isChecked() ? 1 : 0;
        int viBilinearFilteringValue = this->viBilinearFilteringCheckBox->isChecked() ? 1 : 0;
        int viGammaDitherValue = this->viGammaDitherCheckBox->isChecked() ? 1 : 0;
        int nativeTextRectsValue = this->nativeTextRectsCheckBox->isChecked() ? 1 : 0;
        int verticalSyncValue = this->verticalSyncCheckBox->isChecked() ? 1 : 0;

        ConfigSetParameter(configVideoParallel, KEY_SSDITHER, M64TYPE_BOOL, &superSampledDitherValue);
        ConfigSetParameter(configVideoParallel, KEY_AA, M64TYPE_BOOL, &viAaValue);
        ConfigSetParameter(configVideoParallel, KEY_DIVOT, M64TYPE_BOOL, &viDivotFilterValue);
        ConfigSetParameter(configVideoParallel, KEY_VIDITHER, M64TYPE_BOOL, &viDeDitheringValue);
        ConfigSetParameter(configVideoParallel, KEY_NATIVETEXTLOD, M64TYPE_BOOL, &nativeTextureLodValue);
        ConfigSetParameter(configVideoParallel, KEY_WIDESCREEN, M64TYPE_BOOL, &forceWidescreenValue);
        ConfigSetParameter(configVideoParallel, KEY_SSREADBACKS, M64TYPE_BOOL, &supersampledRdramReadsValue);
        ConfigSetParameter(configVideoParallel, KEY_VIBILERP, M64TYPE_BOOL, &viBilinearFilteringValue);
        ConfigSetParameter(configVideoParallel, KEY_GAMMADITHER, M64TYPE_BOOL, &viGammaDitherValue);
        ConfigSetParameter(configVideoParallel, KEY_NATIVETEXTRECT, M64TYPE_BOOL, &nativeTextRectsValue);
        ConfigSetParameter(configVideoParallel, KEY_VSYNC, M64TYPE_BOOL, &verticalSyncValue);

        // shader preset
        QByteArray shaderPreset = this->getShaderPreset().toUtf8();
        ConfigSetParameter(configVideoParallel, KEY_SHADERPRESET, M64TYPE_STRING, shaderPreset.constData());
        int shaderFullWindowValue = this->shaderFullWindowCheckBox->isChecked() ? 1 : 0;
        ConfigSetParameter(configVideoParallel, KEY_SHADERFULLWINDOW, M64TYPE_BOOL, &shaderFullWindowValue);

        ConfigSaveSection("Video-Parallel");
    }
    else if (pushButton == defaultButton)
    {
        this->screenSizeComboBox->setCurrentText("640 x 480");
        this->resolutionUpscaleComboBox->setCurrentText("None");
        this->deinterlacerComboBox->setCurrentIndex(0);
        this->downscalerComboBox->setCurrentIndex(0);
        this->verticalStretchSpinBox->setValue(0);
        this->overscanGroupBox->setChecked(false);
        this->overscanLeftSpinBox->setValue(0);
        this->overscanRightSpinBox->setValue(0);
        this->overscanTopSpinBox->setValue(0);
        this->overscanBottomSpinBox->setValue(0);

        this->superSampledDitherCheckBox->setChecked(true);
        this->viAaCheckBox->setChecked(true);
        this->viDivotFilterCheckBox->setChecked(true);
        this->viDeDitheringCheckBox->setChecked(true);
        this->nativeTextureLodCheckBox->setChecked(false);
        this->forceWidescreenCheckBox->setChecked(false);
        this->supersampledRdramReadsCheckBox->setChecked(false);
        this->viBilinearFilteringCheckBox->setChecked(true);
        this->viGammaDitherCheckBox->setChecked(true);
        this->nativeTextRectsCheckBox->setChecked(true);
        this->verticalSyncCheckBox->setChecked(false);
        this->setShaderPreset(QString());
        this->shaderFullWindowCheckBox->setChecked(false);
    }
}
