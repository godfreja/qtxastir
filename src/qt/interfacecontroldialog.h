/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
 * $Id: interfacecontroldialog.h,v 1.1.2.1 2013/01/30 23:47:23 we7u Exp $
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2013  The Xastir Group
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Look at the README for more information on the program.
 */

#ifndef INTERFACECONTROLDIALOG_H
#define INTERFACECONTROLDIALOG_H

#include <QDialog>
#include "interfacemanager.h"

namespace Ui {
class InterfaceControlDialog;
}

class InterfaceControlDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit InterfaceControlDialog(InterfaceManager& manager, QWidget *parent = 0 );
    ~InterfaceControlDialog();
    
public slots:
    void addInterfaceAction();
    void startAllAction();
    void stopAllAction();
    void startAction();
    void stopAction();
    void preferencesAction();
    void selectedRowChanged(int row);
    void managerAddedInterface(PacketInterface *newInterface);
    void interfaceStatusChanged(PacketInterface *iface, PacketInterface::Device_Status state);

private:
    Ui::InterfaceControlDialog *ui;
    InterfaceManager& theManager;
    void updateButtonsState();
};

#endif // INTERFACECONTROLDIALOG_H
