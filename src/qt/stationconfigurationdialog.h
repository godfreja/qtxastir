#ifndef STATIONCONFIGURATIONDIALOG_H
#define STATIONCONFIGURATIONDIALOG_H

#include <QDialog>

namespace Ui {
class StationConfigurationDialog;
}

class StationConfigurationDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit StationConfigurationDialog(QWidget *parent = 0);
    ~StationConfigurationDialog();
    
private slots:
    void disablePHBChanged(int state);

private:
    Ui::StationConfigurationDialog *ui;
};

#endif // STATIONCONFIGURATIONDIALOG_H
