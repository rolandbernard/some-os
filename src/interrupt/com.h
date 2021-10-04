#ifndef _INT_COM_H_
#define _INT_COM_H_

void sendMachineSoftwareInterrupt(int hart);

void clearMachineSoftwareInterrupt(int hart);

void handleMachineSoftwareInterrupt();

#endif
