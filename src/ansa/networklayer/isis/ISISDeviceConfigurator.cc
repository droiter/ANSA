//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 

#include "ansa/networklayer/isis/ISISDeviceConfigurator.h"

namespace inet {

ISISDeviceConfigurator::ISISDeviceConfigurator() {
    deviceId = nullptr;
    deviceType = nullptr;
    configFile = nullptr;

}

ISISDeviceConfigurator::~ISISDeviceConfigurator() {
    // TODO Auto-generated destructor stub
}



ISISDeviceConfigurator::ISISDeviceConfigurator(const char* devId,
        const char* devType, cXMLElement* confFile, IInterfaceTable* intf)
: deviceId(devId), deviceType(devType), configFile(confFile), ift(intf)
{
}


/* IS-IS related methods */

void ISISDeviceConfigurator::loadISISConfig(ISIS *isisModule, ISIS::ISIS_MODE isisMode){

    /* init module pointers based on isisMode */

    if(isisMode == ISIS::L2_ISIS_MODE){
        //TRILL
        //TODO A! Uncomment after adding TRILL
//        isisModule->setTrill(TRILLAccess().get());


        //RBridgeSplitter


    }else if(isisMode == ISIS::L3_ISIS_MODE){
        //IPv4
        //TODO C2

        //IPv6
        //TODO C2

    }else{
        throw cRuntimeError("Unsupported IS-IS mode");
    }

    //TODO A1 Look into it if it is necessary
//    //CLNSTable must be present in both modes
//    isisModule->setClnsTable(CLNSTableAccess().get());
//
//    //InterfaceTable
//    isisModule->setIft(InterfaceTableAccess().get());
//
//    //NotificationBoard
//    isisModule->setNb(NotificationBoardAccess().get());
//    isisModule->subscribeNb();

    /* end of module's pointers init */



    if(device == NULL){
        if(isisMode == ISIS::L3_ISIS_MODE){
            /* In L3 mode we need configuration (at least NET) */
            throw cRuntimeError("No configuration found for this device");
        }else{
            /* For L2 mode we load defaults ...
             * ... repeat after me zero-configuration. */
            this->loadISISCoreDefaultConfig(isisModule);
            this->loadISISInterfacesConfig(isisModule);
        }
        return;
    }

    cXMLElement *isisRouting = getIsisRouting(device);
    if (isisRouting == NULL)
    {
        if(isisMode == ISIS::L3_ISIS_MODE){
            throw cRuntimeError("Can't find ISISRouting in config file");
        }
    }


    if (isisRouting != NULL)
    {
        //NET
        //TODO: multiple NETs for migrating purposes (merging, splitting areas)
        const char *netAddr = this->getISISNETAddress(isisRouting);
        if (netAddr != NULL)
        {
            isisModule->setNetAddr(netAddr);
        }
        else if (isisMode == ISIS::L2_ISIS_MODE)
        {
            isisModule->generateNetAddr();
        }
        else
        {
            throw cRuntimeError("Net address wasn't specified in IS-IS routing.");
        }

        //IS type {L1(L2_ISIS_MODE) | L2 | L1L2 default for L3_ISIS_MODE}
        if (isisMode == ISIS::L2_ISIS_MODE)
        {
            isisModule->setIsType(L1_TYPE);
        }
        else
        {
            isisModule->setIsType(this->getISISISType(isisRouting));
        }

        //L1 Hello interval
        isisModule->setL1HelloInterval(this->getISISL1HelloInterval(isisRouting));

        //L1 Hello multiplier
        isisModule->setL1HelloMultiplier(this->getISISL1HelloMultiplier(isisRouting));

        //L2 Hello interval
        isisModule->setL2HelloInterval(this->getISISL2HelloInterval(isisRouting));

        //L2 Hello multiplier
        isisModule->setL2HelloMultiplier(this->getISISL2HelloMultiplier(isisRouting));

        //LSP interval
        isisModule->setLspInterval(this->getISISLSPInterval(isisRouting));

        //LSP refresh interval
        isisModule->setLspRefreshInterval(this->getISISLSPRefreshInterval(isisRouting));

        //LSP max lifetime
        isisModule->setLspMaxLifetime(this->getISISLSPMaxLifetime(isisRouting));

        //L1 LSP generating interval
        isisModule->setL1LspGenInterval(this->getISISL1LSPGenInterval(isisRouting));

        //L2 LSP generating interval
        isisModule->setL2LspGenInterval(this->getISISL2LSPGenInterval(isisRouting));

        //L1 LSP send interval
        isisModule->setL1LspSendInterval(this->getISISL1LSPSendInterval(isisRouting));

        //L2 LSP send interval
        isisModule->setL2LspSendInterval(this->getISISL2LSPSendInterval(isisRouting));

        //L1 LSP initial waiting period
        isisModule->setL1LspInitWait(this->getISISL1LSPInitWait(isisRouting));

        //L2 LSP initial waiting period
        isisModule->setL2LspInitWait(this->getISISL2LSPInitWait(isisRouting));

        //L1 CSNP interval
        isisModule->setL1CSNPInterval(this->getISISL1CSNPInterval(isisRouting));

        //L2 CSNP interval
        isisModule->setL2CSNPInterval(this->getISISL2CSNPInterval(isisRouting));

        //L1 PSNP interval
        isisModule->setL1PSNPInterval(this->getISISL1PSNPInterval(isisRouting));

        //L2 PSNP interval
        isisModule->setL2PSNPInterval(this->getISISL2PSNPInterval(isisRouting));

        //L1 SPF Full interval
        isisModule->setL1SpfFullInterval(this->getISISL1SPFFullInterval(isisRouting));

        //L2 SPF Full interval
        isisModule->setL2SpfFullInterval(this->getISISL2SPFFullInterval(isisRouting));

        /* End of core module properties */
    }
    else
    {
        this->loadISISCoreDefaultConfig(isisModule);

    }
    /* Load configuration for interfaces */

    this->loadISISInterfacesConfig(isisModule);
    /* End of load configuration for interfaces */

}


void ISISDeviceConfigurator::loadISISInterfacesConfig(ISIS *isisModule){

    cXMLElement *interfaces = NULL;
    if (device != NULL)
    {
        interfaces = device->getFirstChildWithTag("Interfaces");
        if (interfaces == NULL)
        {
            EV
                    << "deviceId " << deviceId << ": <Interfaces></Interfaces> tag is missing in configuration file: \""
                            << configFile << "\"\n";
//        return;
        }
    }
    // add all interfaces to ISISIft vector containing additional information
//    InterfaceEntry *entryIFT = new InterfaceEntry(this); //TODO added "this" -> experimental
    for (int i = 0; i < ift->getNumInterfaces(); i++)
    {
        InterfaceEntry *ie = ift->getInterface(i);
        if (interfaces == NULL)
        {
            this->loadISISInterfaceDefaultConfig(isisModule, ie);
        }
        else
        {
            this->loadISISInterfaceConfig(isisModule, ie,
                    interfaces->getFirstChildWithAttribute("Interface", "name", ie->getName()));

        }
    }
}


void ISISDeviceConfigurator::loadISISCoreDefaultConfig(ISIS *isisModule){
    //NET

          isisModule->generateNetAddr();


      //IS type {L1(L2_ISIS_MODE) | L2 | L1L2 default for L3_ISIS_MODE}

      isisModule->setIsType(L1_TYPE);

      //set Attach flag
      isisModule->setAtt(false);


      //L1 Hello interval
      isisModule->setL1HelloInterval(ISIS_HELLO_INTERVAL);

      //L1 Hello multiplier
      isisModule->setL1HelloMultiplier(ISIS_HELLO_MULTIPLIER);

      //L2 Hello interval
      isisModule->setL2HelloInterval(ISIS_HELLO_INTERVAL);

      //L2 Hello multiplier
      isisModule->setL2HelloMultiplier(ISIS_HELLO_MULTIPLIER);

      //LSP interval
      isisModule->setLspInterval(ISIS_LSP_INTERVAL);

      //LSP refresh interval
      isisModule->setLspRefreshInterval(ISIS_LSP_REFRESH_INTERVAL);

      //LSP max lifetime
      isisModule->setLspMaxLifetime(ISIS_LSP_MAX_LIFETIME);

      //L1 LSP generating interval
      isisModule->setL1LspGenInterval(ISIS_LSP_GEN_INTERVAL);

      //L2 LSP generating interval
      isisModule->setL2LspGenInterval(ISIS_LSP_GEN_INTERVAL);

      //L1 LSP send interval
      isisModule->setL1LspSendInterval(ISIS_LSP_SEND_INTERVAL);

      //L2 LSP send interval
      isisModule->setL2LspSendInterval(ISIS_LSP_SEND_INTERVAL);

      //L1 LSP initial waiting period
      isisModule->setL1LspInitWait(ISIS_LSP_INIT_WAIT);

      //L2 LSP initial waiting period
      isisModule->setL2LspInitWait(ISIS_LSP_INIT_WAIT);

      //L1 CSNP interval
      isisModule->setL1CSNPInterval(ISIS_CSNP_INTERVAL);

      //L2 CSNP interval
      isisModule->setL2CSNPInterval(ISIS_CSNP_INTERVAL);

      //L1 PSNP interval
      isisModule->setL1PSNPInterval(ISIS_PSNP_INTERVAL);

      //L2 PSNP interval
      isisModule->setL2PSNPInterval(ISIS_PSNP_INTERVAL);

      //L1 SPF Full interval
      isisModule->setL1SpfFullInterval(ISIS_SPF_FULL_INTERVAL);

      //L2 SPF Full interval
      isisModule->setL2SpfFullInterval(ISIS_SPF_FULL_INTERVAL);
}


void ISISDeviceConfigurator::loadISISInterfaceDefaultConfig(ISIS *isisModule, InterfaceEntry *ie){

    ISISInterfaceData *d = new ISISInterfaceData();
        ISISinterface newIftEntry;
        newIftEntry.intID = ie->getInterfaceId();
        d->setIfaceId(ie->getInterfaceId());

        newIftEntry.gateIndex = ie->getNetworkLayerGateIndex();
        d->setGateIndex(ie->getNetworkLayerGateIndex());
        EV <<"deviceId: " << this->deviceId << "ISIS: adding interface, gateIndex: " <<newIftEntry.gateIndex <<endl;

        //set interface priority
        newIftEntry.priority = ISIS_DIS_PRIORITY;  //default value
        d->setPriority(ISIS_DIS_PRIORITY);

        /* Interface is NOT enabled by default. If ANY IS-IS related property is configured on interface then it's enabled. */
        newIftEntry.ISISenabled = false;
        d->setIsisEnabled(false);
        if(isisModule->getMode() == ISIS::L2_ISIS_MODE){
            newIftEntry.ISISenabled = true;
            d->setIsisEnabled(true);
        }

        //set network type (point-to-point vs. broadcast)
        newIftEntry.network = true; //default value means broadcast TODO check with TRILL default values
        d->setNetwork(ISIS_NETWORK_BROADCAST);

        //set interface metric

        newIftEntry.metric = ISIS_METRIC;    //default value
        d->setMetric(ISIS_METRIC);

        //set interface type according to global router configuration
        newIftEntry.circuitType = isisModule->getIsType();
        d->setCircuitType((ISISCircuitType)isisModule->getIsType()); //TODO B1


        //set L1 hello interval in seconds
        newIftEntry.L1HelloInterval = isisModule->getL1HelloInterval();
        d->setL1HelloInterval(isisModule->getL1HelloInterval());

        //set L1 hello multiplier
        newIftEntry.L1HelloMultiplier = isisModule->getL1HelloMultiplier();
        d->setL1HelloMultiplier(isisModule->getL1HelloMultiplier());


        //set L2 hello interval in seconds
        newIftEntry.L2HelloInterval = isisModule->getL2HelloInterval();
        d->setL2HelloInterval(isisModule->getL2HelloInterval());

        //set L2 hello multiplier
        newIftEntry.L2HelloMultiplier = isisModule->getL2HelloMultiplier();
        d->setL2HelloMultiplier(isisModule->getL2HelloMultiplier());

        //set lspInterval
        newIftEntry.lspInterval = isisModule->getLspInterval();
        d->setLspInterval(isisModule->getLspInterval());

        //set L1CsnpInterval
        newIftEntry.L1CsnpInterval = isisModule->getL1CsnpInterval();
        d->setL1CsnpInterval(isisModule->getL1CsnpInterval());

        //set L2CsnpInterval
        newIftEntry.L2CsnpInterval = isisModule->getL2CsnpInterval();
        d->setL2CsnpInterval(isisModule->getL2CsnpInterval());

        //set L1PsnpInterval
        newIftEntry.L1PsnpInterval = isisModule->getL1PsnpInterval();
        d->setL1PsnpInterval(isisModule->getL1PsnpInterval());

        //set L2PsnpInterval
        newIftEntry.L2PsnpInterval = isisModule->getL2PsnpInterval();
        d->setL2PsnpInterval(isisModule->getL2PsnpInterval());

        // priority is not needed for point-to-point, but it won't hurt
        // set priority of current DIS = me at start
        newIftEntry.L1DISpriority = newIftEntry.priority;
        d->setL1DisPriority(d->getPriority());
        newIftEntry.L2DISpriority = newIftEntry.priority;
        d->setL2DisPriority(d->getPriority());

        //set initial designated IS as himself

        memcpy(newIftEntry.L1DIS,isisModule->getSysId(), ISIS_SYSTEM_ID);
        //set LAN identifier; -99 is because, OMNeT starts numbering interfaces from 100 -> interfaceID 100 means LAN ID 0; and we want to start numbering from 1
        //newIftEntry.L1DIS[6] = ie->getInterfaceId() - 99;
        newIftEntry.L1DIS[ISIS_SYSTEM_ID] = isisModule->getISISIftSize() + 1;

        d->setL1Dis(newIftEntry.L1DIS);
        //do the same for L2 DIS

        memcpy(newIftEntry.L2DIS,isisModule->getSysId(), ISIS_SYSTEM_ID);
        //newIftEntry.L2DIS[6] = ie->getInterfaceId() - 99;
        newIftEntry.L2DIS[ISIS_SYSTEM_ID] = isisModule->getISISIftSize() + 1;
        d->setL2Dis(newIftEntry.L2DIS);

        /* By this time the trillData should be initialized.
         * So set the intial appointedForwaders to itself for configured VLAN(s).
         * TODO B5 add RFC reference and do some magic with vlanId, desiredVlanId, enabledVlans, ... */
        //TODO A! Uncomment after adding TRILL
//        if(isisModule->getMode() == ISIS::L2_ISIS_MODE){
//            ie->trillData()->addAppointedForwarder( ie->trillData()->getVlanId(), isisModule->getNickname());
//        }
        newIftEntry.passive = false;
        d->setPassive(false);
        newIftEntry.entry = ie;

    //    this->ISISIft.push_back(newIftEntry);
        isisModule->appendISISInterface(newIftEntry);
        ie->setISISInterfaceData(d);
}


void ISISDeviceConfigurator::loadISISInterfaceConfig(ISIS *isisModule, InterfaceEntry *entry, cXMLElement *intElement){


    if(intElement == NULL){

        this->loadISISInterfaceDefaultConfig(isisModule, entry);
        return;
    }
    ISISinterface newIftEntry;
    newIftEntry.intID = entry->getInterfaceId();

    newIftEntry.gateIndex = entry->getNetworkLayerGateIndex();
    EV <<"deviceId: " << this->deviceId << "ISIS: adding interface, gateIndex: " <<newIftEntry.gateIndex <<endl;

    //set interface priority
    newIftEntry.priority = ISIS_DIS_PRIORITY;  //default value

    /* Interface is NOT enabled by default. If ANY IS-IS related property is configured on interface then it's enabled. */
    newIftEntry.ISISenabled = false;
    if(isisModule->getMode() == ISIS::L2_ISIS_MODE){
        newIftEntry.ISISenabled = true;
    }

    cXMLElement *priority = intElement->getFirstChildWithTag("ISIS-Priority");
    if (priority != NULL && priority->getNodeValue() != NULL)
    {
        newIftEntry.priority = (unsigned char) atoi(priority->getNodeValue());
        newIftEntry.ISISenabled = true;
    }


    //set network type (point-to-point vs. broadcast)

    newIftEntry.network = true; //default value

    cXMLElement *network = intElement->getFirstChildWithTag("ISIS-Network");
    if (network != NULL && network->getNodeValue() != NULL)
    {
        if (!strcmp(network->getNodeValue(), "point-to-point"))
        {
            newIftEntry.network = false;
            EV << "Interface network type is point-to-point " << network->getNodeValue() << endl;
        }
        else if (!strcmp(network->getNodeValue(), "broadcast"))
        {
            EV << "Interface network type is broadcast " << network->getNodeValue() << endl;
        }
        else
        {
            EV << "ERORR: Unrecognized interface's network type: " << network->getNodeValue() << endl;

        }
        newIftEntry.ISISenabled = true;

    }



    //set interface metric

    newIftEntry.metric = ISIS_METRIC;    //default value

        cXMLElement *metric = intElement->getFirstChildWithTag("ISIS-Metric");
        if(metric != NULL && metric->getNodeValue() != NULL)
        {
            newIftEntry.metric = (unsigned char) atoi(metric->getNodeValue());
            newIftEntry.ISISenabled = true;
        }




    //set interface type according to global router configuration
    switch(isisModule->getIsType())
    {
        case(L1_TYPE):
                 newIftEntry.circuitType = L1_TYPE;
                 break;
        case(L2_TYPE):
                 newIftEntry.circuitType = L2_TYPE;
                 break;
        //if router is type is equal L1L2, then interface configuration sets the type
        default: {

            newIftEntry.circuitType = L1L2_TYPE;

            cXMLElement *circuitType = intElement->getFirstChildWithTag("ISIS-Circuit-Type");
            if (circuitType != NULL && circuitType->getNodeValue() != NULL)
            {
                if (strcmp(circuitType->getNodeValue(), "L2") == 0){
                    newIftEntry.circuitType = L2_TYPE;
                }
                else
                {
                    if (strcmp(circuitType->getNodeValue(), "L1") == 0)
                        newIftEntry.circuitType = L1_TYPE;
                }
                newIftEntry.ISISenabled = true;
            }
            else
            {
                newIftEntry.circuitType = L1L2_TYPE;
            }

            break;
        }
    }

    //set L1 hello interval in seconds
    cXMLElement *L1HelloInt = intElement->getFirstChildWithTag(
            "ISIS-L1-Hello-Interval");
    if (L1HelloInt == NULL || L1HelloInt->getNodeValue() == NULL) {
        newIftEntry.L1HelloInterval = isisModule->getL1HelloInterval();
    } else {
        newIftEntry.L1HelloInterval = atoi(L1HelloInt->getNodeValue());
    }

    //set L1 hello multiplier
    cXMLElement *L1HelloMult = intElement->getFirstChildWithTag(
            "ISIS-L1-Hello-Multiplier");
    if (L1HelloMult == NULL || L1HelloMult->getNodeValue() == NULL) {
        newIftEntry.L1HelloMultiplier = isisModule->getL1HelloMultiplier();
    } else {
        newIftEntry.L1HelloMultiplier = atoi(L1HelloMult->getNodeValue());
    }

    //set L2 hello interval in seconds
    cXMLElement *L2HelloInt = intElement->getFirstChildWithTag(
            "ISIS-L2-Hello-Interval");
    if (L2HelloInt == NULL || L2HelloInt->getNodeValue() == NULL) {
        newIftEntry.L2HelloInterval = isisModule->getL2HelloInterval();
    } else {
        newIftEntry.L2HelloInterval = atoi(L2HelloInt->getNodeValue());
    }

    //set L2 hello multiplier
    cXMLElement *L2HelloMult = intElement->getFirstChildWithTag(
            "ISIS-L2-Hello-Multiplier");
    if (L2HelloMult == NULL || L2HelloMult->getNodeValue() == NULL) {
        newIftEntry.L2HelloMultiplier = isisModule->getL2HelloMultiplier();
    } else {
        newIftEntry.L2HelloMultiplier = atoi(L2HelloMult->getNodeValue());
    }

    //set lspInterval
    cXMLElement *cxlspInt = intElement->getFirstChildWithTag("ISIS-LSP-Interval");
    if (cxlspInt == NULL || cxlspInt->getNodeValue() == NULL)
    {
//        newIftEntry.lspInterval = ISIS_LSP_INTERVAL;
        newIftEntry.lspInterval = isisModule->getLspInterval();
    }
    else
    {
        newIftEntry.lspInterval = atoi(cxlspInt->getNodeValue());
    }

    //set L1CsnpInterval
    cXMLElement *cxL1CsnpInt = intElement->getFirstChildWithTag("ISIS-L1-CSNP-Interval");
    if (cxL1CsnpInt == NULL || cxL1CsnpInt->getNodeValue() == NULL)
    {
//        newIftEntry.L1CsnpInterval = ISIS_CSNP_INTERVAL;
                newIftEntry.L1CsnpInterval = isisModule->getL1CsnpInterval();
    }
    else
    {
        newIftEntry.L1CsnpInterval = atoi(cxL1CsnpInt->getNodeValue());
    }

    //set L2CsnpInterval
    cXMLElement *cxL2CsnpInt = intElement->getFirstChildWithTag("ISIS-L2-CSNP-Interval");
    if (cxL2CsnpInt == NULL || cxL2CsnpInt->getNodeValue() == NULL)
    {
//        newIftEntry.L2CsnpInterval = ISIS_CSNP_INTERVAL;
        newIftEntry.L2CsnpInterval = isisModule->getL2CsnpInterval();
    }
    else
    {
        newIftEntry.L2CsnpInterval = atoi(cxL2CsnpInt->getNodeValue());
    }

    //set L1PsnpInterval
    cXMLElement *cxL1PsnpInt = intElement->getFirstChildWithTag("ISIS-L1-PSNP-Interval");
    if (cxL1PsnpInt == NULL || cxL1PsnpInt->getNodeValue() == NULL)
    {
//        newIftEntry.L1PsnpInterval = ISIS_PSNP_INTERVAL;
        newIftEntry.L1PsnpInterval = isisModule->getL1PsnpInterval();
    }
    else
    {
        newIftEntry.L1PsnpInterval = atoi(cxL1PsnpInt->getNodeValue());
    }

    //set L2PsnpInterval
    cXMLElement *cxL2PsnpInt = intElement->getFirstChildWithTag("ISIS-L2-PSNP-Interval");
    if (cxL2PsnpInt == NULL || cxL2PsnpInt->getNodeValue() == NULL)
    {
//        newIftEntry.L2PsnpInterval = ISIS_PSNP_INTERVAL;
        newIftEntry.L2PsnpInterval = isisModule->getL2PsnpInterval();
    }
    else
    {
        newIftEntry.L2PsnpInterval = atoi(cxL2PsnpInt->getNodeValue());
    }


    // priority is not needed for point-to-point, but it won't hurt
    // set priority of current DIS = me at start
    newIftEntry.L1DISpriority = newIftEntry.priority;
    newIftEntry.L2DISpriority = newIftEntry.priority;

    //set initial designated IS as himself
//    this->copyArrayContent((unsigned char*)this->sysId, newIftEntry.L1DIS, ISIS_SYSTEM_ID, 0, 0);
    memcpy(newIftEntry.L1DIS,isisModule->getSysId(), ISIS_SYSTEM_ID);
    //set LAN identifier; -99 is because, OMNeT starts numbering interfaces from 100 -> interfaceID 100 means LAN ID 0; and we want to start numbering from 1
    //newIftEntry.L1DIS[6] = entry->getInterfaceId() - 99;
    newIftEntry.L1DIS[ISIS_SYSTEM_ID] = newIftEntry.gateIndex + 1;
    //do the same for L2 DIS
//    memcpy((unsigned char*)isisModule->getSy, newIftEntry.L2DIS, ISIS_SYSTEM_ID);
    memcpy(newIftEntry.L2DIS,isisModule->getSysId(), ISIS_SYSTEM_ID);
    //newIftEntry.L2DIS[6] = entry->getInterfaceId() - 99;
    newIftEntry.L2DIS[ISIS_SYSTEM_ID] = newIftEntry.gateIndex + 1;

    newIftEntry.passive = false;
    newIftEntry.entry = entry;

//    this->ISISIft.push_back(newIftEntry);
    isisModule->appendISISInterface(newIftEntry);
}




const char *ISISDeviceConfigurator::getISISNETAddress(cXMLElement *isisRouting)
{
    //TODO: multiple NETs for migrating purposes (merging, splitting areas)
    cXMLElement *net = isisRouting->getFirstChildWithTag("NET");
    if (net == NULL)
    {
//            EV << "deviceId " << deviceId << ": Net address wasn't specified in IS-IS routing\n";
//            throw cRuntimeError("Net address wasn't specified in IS-IS routing");
        return NULL;
    }
    return net->getNodeValue();
}

short int ISISDeviceConfigurator::getISISISType(cXMLElement *isisRouting){
    //set router IS type {L1(L2_M | L2 | L1L2 (default)}
    cXMLElement *routertype = isisRouting->getFirstChildWithTag("IS-Type");
    if (routertype == NULL) {
        return L1L2_TYPE;
    } else {
        const char* routerTypeValue = routertype->getNodeValue();
        if (routerTypeValue == NULL) {
            return L1L2_TYPE;
        } else {
            if (strcmp(routerTypeValue, "level-1") == 0) {
                return L1_TYPE;
            } else {
                if (strcmp(routerTypeValue, "level-2") == 0) {
                    return L2_TYPE;
                } else {
                    return L1L2_TYPE;
                }
            }
        }
    }
}

int ISISDeviceConfigurator::getISISL1HelloInterval(cXMLElement *isisRouting){
    //get L1 hello interval in seconds
    cXMLElement *L1HelloInt = isisRouting->getFirstChildWithTag(
            "L1-Hello-Interval");
    if (L1HelloInt == NULL || L1HelloInt->getNodeValue() == NULL) {
        return ISIS_HELLO_INTERVAL;
    } else {
        return atoi(L1HelloInt->getNodeValue());
    }
}

int ISISDeviceConfigurator::getISISL1HelloMultiplier(cXMLElement *isisRouting){
    //get L1 hello multiplier
    cXMLElement *L1HelloMult = isisRouting->getFirstChildWithTag(
            "L1-Hello-Multiplier");
    if (L1HelloMult == NULL || L1HelloMult->getNodeValue() == NULL) {
        return ISIS_HELLO_MULTIPLIER;
    } else {
        return atoi(L1HelloMult->getNodeValue());
    }
}

int ISISDeviceConfigurator::getISISL2HelloInterval(cXMLElement *isisRouting){
    //get L2 hello interval in seconds
    cXMLElement *L2HelloInt = isisRouting->getFirstChildWithTag(
            "L2-Hello-Interval");
    if (L2HelloInt == NULL || L2HelloInt->getNodeValue() == NULL) {
        return ISIS_HELLO_INTERVAL;
    } else {
        return atoi(L2HelloInt->getNodeValue());
    }
}

int ISISDeviceConfigurator::getISISL2HelloMultiplier(cXMLElement *isisRouting){
    //get L2 hello multiplier
    cXMLElement *L2HelloMult = isisRouting->getFirstChildWithTag(
            "L2-Hello-Multiplier");
    if (L2HelloMult == NULL || L2HelloMult->getNodeValue() == NULL) {
        return ISIS_HELLO_MULTIPLIER;
    } else {
        return atoi(L2HelloMult->getNodeValue());
    }
}

int ISISDeviceConfigurator::getISISLSPInterval(cXMLElement *isisRouting){
    //set lspInterval
    cXMLElement *cxlspInt = isisRouting->getFirstChildWithTag("LSP-Interval");
    if (cxlspInt == NULL || cxlspInt->getNodeValue() == NULL)
    {
        return ISIS_LSP_INTERVAL;
    }
    else
    {
        return atoi(cxlspInt->getNodeValue());
    }
}

int ISISDeviceConfigurator::getISISLSPRefreshInterval(cXMLElement *isisRouting){
    //get lspRefreshInterval
    cXMLElement *cxlspRefInt = isisRouting->getFirstChildWithTag("LSP-Refresh-Interval");
    if (cxlspRefInt == NULL || cxlspRefInt->getNodeValue() == NULL)
    {
        return ISIS_LSP_REFRESH_INTERVAL;
    }
    else
    {
        return atoi(cxlspRefInt->getNodeValue());
    }
}

int ISISDeviceConfigurator::getISISLSPMaxLifetime(cXMLElement *isisRouting)
{
    //get lspMaxLifetime
    cXMLElement *cxlspMaxLife = isisRouting->getFirstChildWithTag("LSP-Max-Lifetime");
    if (cxlspMaxLife == NULL || cxlspMaxLife->getNodeValue() == NULL)
    {
        return ISIS_LSP_MAX_LIFETIME;
    }
    else
    {
        return atoi(cxlspMaxLife->getNodeValue());
    }

}

int ISISDeviceConfigurator::getISISL1LSPGenInterval(cXMLElement *isisRouting)
{
    //set L1LspGenInterval (CISCO's
    cXMLElement *cxL1lspGenInt = isisRouting->getFirstChildWithTag("L1-LSP-Gen-Interval");
    if (cxL1lspGenInt == NULL || cxL1lspGenInt->getNodeValue() == NULL)
    {
        return ISIS_LSP_GEN_INTERVAL;
    }
    else
    {
        return atoi(cxL1lspGenInt->getNodeValue());
    }
}

int ISISDeviceConfigurator::getISISL2LSPGenInterval(cXMLElement *isisRouting)
{
    //get L2LspGenInterval (CISCO's
    cXMLElement *cxL2lspGenInt = isisRouting->getFirstChildWithTag("L2-LSP-Gen-Interval");
    if (cxL2lspGenInt == NULL || cxL2lspGenInt->getNodeValue() == NULL)
    {
        return ISIS_LSP_GEN_INTERVAL;
    }
    else
    {
        return atoi(cxL2lspGenInt->getNodeValue());
    }
}

int ISISDeviceConfigurator::getISISL1LSPSendInterval(cXMLElement *isisRouting){
    //get L1LspSendInterval
    cXMLElement *cxL1lspSendInt = isisRouting->getFirstChildWithTag("L1-LSP-Send-Interval");
    if (cxL1lspSendInt == NULL || cxL1lspSendInt->getNodeValue() == NULL)
    {
        return ISIS_LSP_SEND_INTERVAL;
    }
    else
    {
        return atoi(cxL1lspSendInt->getNodeValue());
    }
}

int ISISDeviceConfigurator::getISISL2LSPSendInterval(cXMLElement *isisRouting){
    //get L2LspSendInterval
    cXMLElement *cxL2lspSendInt = isisRouting->getFirstChildWithTag("L2-LSP-Send-Interval");
    if (cxL2lspSendInt == NULL || cxL2lspSendInt->getNodeValue() == NULL)
    {
        return ISIS_LSP_SEND_INTERVAL;
    }
    else
    {
        return atoi(cxL2lspSendInt->getNodeValue());
    }
}

int ISISDeviceConfigurator::getISISL1LSPInitWait(cXMLElement *isisRouting)
{
    //get L1LspInitWait
    cXMLElement *cxL1lspInitWait = isisRouting->getFirstChildWithTag("L1-LSP-Init-Wait");
    if (cxL1lspInitWait == NULL || cxL1lspInitWait->getNodeValue() == NULL)
    {
        return ISIS_LSP_INIT_WAIT;
    }
    else
    {
        return atoi(cxL1lspInitWait->getNodeValue());
    }
}


int ISISDeviceConfigurator::getISISL2LSPInitWait(cXMLElement *isisRouting)
{
    //get L2LspInitWait
    cXMLElement *cxL2lspInitWait = isisRouting->getFirstChildWithTag("L2-LSP-Init-Wait");
    if (cxL2lspInitWait == NULL || cxL2lspInitWait->getNodeValue() == NULL)
    {
        return ISIS_LSP_INIT_WAIT;
    }
    else
    {
        return atoi(cxL2lspInitWait->getNodeValue());
    }
}

int ISISDeviceConfigurator::getISISL1CSNPInterval(cXMLElement *isisRouting){
    //get L1CsnpInterval
    cXMLElement *cxL1CsnpInt = isisRouting->getFirstChildWithTag("L1-CSNP-Interval");
    if (cxL1CsnpInt == NULL || cxL1CsnpInt->getNodeValue() == NULL)
    {
        return ISIS_CSNP_INTERVAL;
    }
    else
    {
        return atoi(cxL1CsnpInt->getNodeValue());
    }

}

int ISISDeviceConfigurator::getISISL2CSNPInterval(cXMLElement *isisRouting){
    //get L2CsnpInterval
    cXMLElement *cxL2CsnpInt = isisRouting->getFirstChildWithTag("L2-CSNP-Interval");
    if (cxL2CsnpInt == NULL || cxL2CsnpInt->getNodeValue() == NULL)
    {
        return ISIS_CSNP_INTERVAL;
    }
    else
    {
        return atoi(cxL2CsnpInt->getNodeValue());
    }

}

int ISISDeviceConfigurator::getISISL1PSNPInterval(cXMLElement *isisRouting){
    //get L1PSNPInterval
    cXMLElement *cxL1PSNPInt = isisRouting->getFirstChildWithTag("L1-PSNP-Interval");
    if (cxL1PSNPInt == NULL || cxL1PSNPInt->getNodeValue() == NULL)
    {
        return ISIS_PSNP_INTERVAL;
    }
    else
    {
        return atoi(cxL1PSNPInt->getNodeValue());
    }

}

int ISISDeviceConfigurator::getISISL2PSNPInterval(cXMLElement *isisRouting){
    //get L2PSNPInterval
    cXMLElement *cxL2PSNPInt = isisRouting->getFirstChildWithTag("L2-PSNP-Interval");
    if (cxL2PSNPInt == NULL || cxL2PSNPInt->getNodeValue() == NULL)
    {
        return ISIS_PSNP_INTERVAL;
    }
    else
    {
        return atoi(cxL2PSNPInt->getNodeValue());
    }

}

int ISISDeviceConfigurator::getISISL1SPFFullInterval(cXMLElement *isisRouting){
    //get L1SPFFullInterval
    cXMLElement *cxL1SPFFullInt = isisRouting->getFirstChildWithTag("L1-SPF-Full-Interval");
    if (cxL1SPFFullInt == NULL || cxL1SPFFullInt->getNodeValue() == NULL)
    {
        return ISIS_SPF_FULL_INTERVAL;
    }
    else
    {
        return atoi(cxL1SPFFullInt->getNodeValue());
    }

}

int ISISDeviceConfigurator::getISISL2SPFFullInterval(cXMLElement *isisRouting){
    //get L2SPFFullInterval
    cXMLElement *cxL2SPFFullInt = isisRouting->getFirstChildWithTag("L2-SPF-Full-Interval");
    if (cxL2SPFFullInt == NULL || cxL2SPFFullInt->getNodeValue() == NULL)
    {
        return ISIS_SPF_FULL_INTERVAL;
    }
    else
    {
        return atoi(cxL2SPFFullInt->getNodeValue());
    }

}


/* Check if IS-IS is enabled in XML config.
 * Return NULL if not presented, otherwise return main IS-IS element
 */
cXMLElement * ISISDeviceConfigurator::getIsisRouting(cXMLElement * device)
{
    if(device == NULL)
        return NULL;

    cXMLElement *routing = device->getFirstChildWithTag("Routing");
    if(routing == NULL)
        return  NULL;

    cXMLElement * isis = routing->getFirstChildWithTag("ISIS");
    return isis;
}

} /* namespace inet */
