/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
 * $Id: netinterfacepropertiesdialog.cpp,v 1.1.2.1 2013/01/30 23:47:23 we7u Exp $
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

#include "netinterfacepropertiesdialog.h"
#include "ui_netinterfacepropertiesdialog.h"

NetInterfacePropertiesDialog::NetInterfacePropertiesDialog(InterfaceManager& manager, NetInterface *interface, bool isItNew, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::NetInterfacePropertiesDialog),
    theManager(manager), theInterface(interface), newInterface(isItNew)
{
    ui->setupUi(this);
    ui->activateOnStartup->setChecked(interface->getActivateOnStartup());
    ui->allowTransmitting->setChecked(interface->transmitAllowed());
    ui->hostEdit->setText(interface->getHostName());
    ui->portEdit->setText(interface->getPortString());
    ui->filterEdit->setText(interface->getFilter());
    ui->callsignEdit->setText(interface->getCallSign());
    ui->passcodeEdit->setText(interface->getPasscode());
}

NetInterfacePropertiesDialog::~NetInterfacePropertiesDialog()
{
    delete ui;
}

void NetInterfacePropertiesDialog::accept()
{
    theInterface->setActivateOnStartup(ui->activateOnStartup->isChecked());
    theInterface->setTransmitAllowed(ui->allowTransmitting->isChecked());
    theInterface->setHostName(ui->hostEdit->text());
    theInterface->setPortString(ui->portEdit->text());
    theInterface->setCallsign(ui->callsignEdit->text());
    theInterface->setPasscode(ui->passcodeEdit->text());
    theInterface->setFilter(ui->filterEdit->text());

    if(newInterface) {
        theManager.addNewInterface(theInterface);
    }
    this->destroy();
}

void NetInterfacePropertiesDialog::reject()
{
    if(newInterface) delete theInterface;
    this->destroy();
}

