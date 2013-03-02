#include "stationconfigurationdialog.h"
#include "ui_stationconfigurationdialog.h"

StationConfigurationDialog::StationConfigurationDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::StationConfigurationDialog)
{
    ui->setupUi(this);
    connect(ui->disablePHGCheckbox,SIGNAL(stateChanged(int)),this,SLOT(disablePHBChanged(int)));
}

StationConfigurationDialog::~StationConfigurationDialog()
{
    delete ui;
}

void StationConfigurationDialog::disablePHBChanged(int state)
{
    ui->powerCombo->setEnabled(!state);
    ui->gainCombo->setEnabled(!state);
    ui->heightCombo->setEnabled(!state);
    ui->directivityCombo->setEnabled(!state);
}
