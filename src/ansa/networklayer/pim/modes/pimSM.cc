// Copyright (C) 2013 Brno University of Technology (http://nes.fit.vutbr.cz/ansa)
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
/**
 * @file pimSM.cc
 * @date 29.10.2011
 * @author: Tomas Prochazka (mailto:xproch21@stud.fit.vutbr.cz), Veronika Rybova, Vladimir Vesely (mailto:ivesely@fit.vutbr.cz)
 * @brief File implements PIM sparse mode.
 * @details Implementation will be done in the future according to RFC4601.
 */

#include "pimSM.h"
#include "deviceConfigurator.h"


Define_Module(pimSM);

/**
 * HANDLE MESSAGE
 *
 * The method is used to handle new messages. Self messages are timer and they are sent to
 * method which processes PIM timers. Other messages should be PIM packets, so they are sent
 * to method which processes PIM packets.
 *
 * @param msg Pointer to new message.
 * @see PIMPacket()
 * @see PIMTimer()
 * @see processPIMTimer()
 * @see processPIMPkt()
 */
void pimSM::handleMessage(cMessage *msg)
{
	EV << "PIMSM::handleMessage" << endl;

	// self message (timer)
	if (msg->isSelfMessage())
	{
	   EV << "PIMSM::handleMessage:Timer" << endl;
	   PIMTimer *timer = check_and_cast <PIMTimer *> (msg);
	   processPIMTimer(timer);
	}
	else if (dynamic_cast<PIMPacket *>(msg))
	{
	   EV << "PIMSM::handleMessage: PIM-SM packet" << endl;
	   PIMPacket *pkt = check_and_cast<PIMPacket *>(msg);
	   EV << "Version: " << pkt->getVersion() << ", type: " << pkt->getType() << endl;
	   processPIMPkt(pkt);
	}
	else
	   EV << "PIMSM::handleMessage: Wrong message" << endl;
}

/**
 * INITIALIZE
 *
 * The method initializes PIM-SM module. It get access to all needed tables and other objects.
 * It subscribes to important notifications. If there is no PIM interface, all module can be
 * disabled. The method also read global pim-sm configuration as RP address and SPT threshold.
 *
 * @param stage Stage of initialization.
 */
void pimSM::initialize(int stage)
{
    if (stage == 4)
    {
        EV << "PIMSM::initialize: entering..." << endl;

        // Pointer to routing tables, interface tables, notification board
        rt = AnsaRoutingTableAccess().get();
        ift = InterfaceTableAccess().get();
        nb = NotificationBoardAccess().get();
        pimIft = PimInterfaceTableAccess().get();
        pimNbt = PimNeighborTableAccess().get();

        // is PIM enabled?
        if (pimIft->getNumInterface() == 0)
        {
            EV << "PIM is NOT enabled on device " << endl;
            return;
        }

        // subscribe for notifications
        nb->subscribe(this, NF_IPv4_NEW_MULTICAST_SPARSE);
        nb->subscribe(this, NF_IPv4_MDATA_REGISTER);
        nb->subscribe(this, NF_IPv4_REC_REGISTER_STOP);
        nb->subscribe(this, NF_IPv4_NEW_IGMP_ADDED_PISM);
        nb->subscribe(this, NF_IPv4_NEW_IGMP_REMOVED_PIMSM);

        DeviceConfigurator *devConf = ModuleAccess<DeviceConfigurator>("deviceConfigurator").get();
        devConf->loadPimGlobalConfig(this);

    }
}

/**
 * CREATE KEEP ALIVE TIMER
 *
 * The method is used to create PIMKeepAliveTimer timer. The timer is set when source of multicast is
 * connected directly to the router.  If timer expires, router will remove the route from multicast
 * routing table. It is set to (S,G).
 *
 * @param source IP address of multicast source.
 * @param group IP address of multicast group.
 * @return Pointer to new Keep Alive Timer
 * @see PIMkat()
 */
PIMkat* pimSM::createKeepAliveTimer(IPv4Address source, IPv4Address group)
{
    PIMkat *timer = new PIMkat();
    timer->setName("PIMKeepAliveTimer");
    timer->setSource(source);
    timer->setGroup(group);
    if (group == IPv4Address::UNSPECIFIED_ADDRESS)      //for (*,G)
        scheduleAt(simTime() + 2*KAT, timer);
    else
        scheduleAt(simTime() + KAT, timer);            //for (S,G)
    return timer;
}

/**
 * CREATE REGISTER-STOP TIMER
 *
 * The method is used to create PIMRegisterStopTimer timer. The timer is used to violate Register-null
 * message to keep Register tunnel disconnected. If timer expires, DR router of source is going to send
 * Register-null message.
 *
 * @param source IP address of multicast source.
 * @param group IP address of multicast group.
 * @return Pointer to new Register Stop Timer
 * @see PIMrst()
 */
PIMrst* pimSM::createRegisterStopTimer(IPv4Address source, IPv4Address group)
{
    PIMrst *timer = new PIMrst();
    timer->setName("PIMRegisterStopTimer");
    timer->setSource(source);
    timer->setGroup(group);

    scheduleAt(simTime() + RST - REGISTER_PROBE_TIME, timer);
    return timer;
}

/**
 * CREATE EXPIRY TIMER
 *
 * The method is used to create PIMExpiryTimer.
 *
 * @param holdtime time to keep route in routing table
 * @param group IP address of multicast group.
 * @return Pointer to new Expiry Timer
 * @see PIMet()
 */
PIMet* pimSM::createExpiryTimer(int holdtime, IPv4Address group, IPv4Address source, int StateType)
{
    PIMet *timer = new PIMet();
    timer->setName("PIMExpiryTimer");
    timer->setGroup(group);
    timer->setSource(source);
    timer->setStateType(StateType);

    scheduleAt(simTime() + holdtime, timer);
    return timer;
}

/**
 * CREATE JOIN TIMER
 *
 * The method is used to create PIMJoinTimer.
 *
 * @param group IP address of multicast group.
 * @param JPaddr joining IP address.
 * @param upstreamNbr IP address of upstream neighbor.
 * @return Pointer to new Join Timer
 * @see PIMjt()
 */
PIMjt* pimSM::createJoinTimer(IPv4Address group, IPv4Address JPaddr, IPv4Address upstreamNbr, int JoinType)
{
    PIMjt *timer = new PIMjt();
    timer->setName("PIMJoinTimer");
    timer->setGroup(group);
    timer->setJoinPruneAddr(JPaddr);
    timer->setUpstreamNbr(upstreamNbr);
    timer->setJoinType(JoinType);

    scheduleAt(simTime() + JT, timer);
    return timer;
}

/**
 * CREATE PRUNE PENDING TIMER
 *
 * The method is used to create PIMPrunePendingTimer.
 *
 * @param group IP address of multicast group.
 * @param JPaddr joining IP address.
 * @param upstreamNbr IP address of upstream neighbor.
 * @return Pointer to new PrunePending Timer
 * @see PIMppt()
 */
PIMppt* pimSM::createPrunePendingTimer(IPv4Address group, IPv4Address JPaddr, IPv4Address upstreamNbr)
{
    PIMppt *timer = new PIMppt();
    timer->setName("PIMPrunePendingTimer");

    timer->setGroup(group);
    timer->setJoinPruneAddr(JPaddr);
    timer->setUpstreamNbr(upstreamNbr);

    scheduleAt(simTime() + PPT, timer);
    return timer;
}

/**
 * SET RP ADDRESS
 *
 * The method is used to set RP address from configuration.
 *
 * @param address in string format.
 * @see getRPAddress()
 */
void pimSM::setRPAddress(std::string address)
{
    if (address != "")
    {
        std::string RP (address);
        RPAddress = IPv4Address(RP.c_str());
    }
    else
        EV << "PIMSM::setRPAddress: empty RP address" << endl;
}

/**
 * SET SPT threshold
 *
 * The method is used to set SPT threshold from configuration.
 *
 * @param threshold in string format.
 * @see getSPTAddress()
 */
void pimSM::setSPTthreshold(std::string threshold)
{
    if (threshold != "")
        SPTthreshold.append(threshold);
    else
        EV << "PIMSM::setSPTthreshold: bad SPTthreshold" << endl;
}

/**
 * I AM RP
 *
 * The method is used to determine if router is Rendezvous Point.
 *
 * @param RPaddress IP address to test.
 */
bool pimSM::IamRP (IPv4Address RPaddress)
{
    InterfaceEntry *intf;

    for (int i=0; i < ift->getNumInterfaces(); i++)
    {
        intf = ift->getInterface(i);
        if (intf->ipv4Data()->getIPAddress() == RPaddress)
            return true;
    }
    return false;
}

/**
 * I AM RP
 *
 * The method is used to determine if router is Designed Router for given IP address.
 *
 * @param sourceAddr IP address to test.
 */
bool pimSM::IamDR (IPv4Address sourceAddr)
{
    InterfaceEntry *intf;
    IPv4Address intfAddr;
    IPv4Address intfMask;

    for (int i=0; i < ift->getNumInterfaces(); i++)
    {
        intf = ift->getInterface(i);
        intfAddr = intf->ipv4Data()->getIPAddress();
        intfMask = intf->ipv4Data()->getNetmask();

        if (IPv4Address::maskedAddrAreEqual(intfAddr,sourceAddr,intfMask))
        {
            EV << "I AM DR for: " << intfAddr << endl;
            return true;
        }
    }
    EV << "I AM NOT DR " << endl;
    return false;
}

/**
 * SET CTRL FOR MESSAGE
 *
 * The method is used to set up ctrl attributes.
 *
 * @param destAddr destination IP address.
 * @param source destination IP address.
 * @param protocol number in integer.
 * @param interfaceId in integer.
 * @return TTL time to live.
 */
IPv4ControlInfo *pimSM::setCtrlForMessage (IPv4Address destAddr,IPv4Address srcAddr,int protocol, int interfaceId, int TTL)
{

    IPv4ControlInfo *ctrl = new IPv4ControlInfo();
    ctrl->setDestAddr(destAddr);
    ctrl->setSrcAddr(srcAddr);
    ctrl->setProtocol(protocol);
    ctrl->setInterfaceId(interfaceId);
    ctrl->setTimeToLive(TTL);

    return ctrl;
}

/**
 * PROCESS KEEP ALIVE TIMER
 *
 * The method is used to process PIM Keep Alive Timer. It is (S,G) timer. When Keep Alive Timer expires,
 * route is removed from multicast routing table.
 *
 * @param timer Pointer to Keep Alive Timer.
 * @see PIMkat()
 */
void pimSM::processKeepAliveTimer(PIMkat *timer)
{
    EV << "pimSM::processKeepAliveTimer: route will be deleted" << endl;
    AnsaIPv4MulticastRoute *route = rt->getRouteFor(timer->getGroup(), timer->getSource());

    delete timer;
    route->setKat(NULL);
    rt->deleteMulticastRoute(route);
}

/**
 * PROCESS REGISTER STOP TIMER
 *
 * The method is used to process PIM Register Stop Timer. RST is used to send
 * periodic Register-Null messages
 *
 * @param timer Pointer to Register Stop Timer.
 * @see PIMrst()
 */
void pimSM::processRegisterStopTimer(PIMrst *timer)
{
    EV << "pimSM::processRegisterStopTimer: " << endl;

    sendPIMRegisterNull(timer->getSource(), timer->getGroup());
    //TODO set RST to probe time, if RST expires, add encapsulation tunnel
}

/**
 * PROCESS EXPIRY TIMER
 *
 * The method is used to process PIM Expiry Timer. It is timer for (S,G) and (*,G).
 * When Expiry timer expires,route is removed from multicast routing table.
 *
 * @param timer Pointer to Keep Alive Timer.
 * @see PIMet()
 */
void pimSM::processExpiryTimer(PIMet *timer)
{
    EV << "pimSM::processExpiryTimer: ";

    if (timer->getStateType() == G)
    {
        EV << " (*,G) "  << endl;
        AnsaIPv4MulticastRoute *routeG = rt->getRouteFor(timer->getGroup(), IPv4Address::UNSPECIFIED_ADDRESS);

        delete timer;
        routeG->setEt(NULL);
        rt->deleteMulticastRoute(routeG);
    }
    else if (timer->getStateType() == SG)
    {
        EV << " (S,G) "  << endl;
        AnsaIPv4MulticastRoute *routeSG = rt->getRouteFor(timer->getGroup(), timer->getSource());
        InterfaceVector outInt = routeSG->getOutInt();

        // FIXME delete only given interface
        for (unsigned k = 0; k < outInt.size(); k++)
                outInt.erase(outInt.begin() + k);
        routeSG->setOutInt(outInt);
    }

}

/**
 * PROCESS JOIN TIMER
 *
 * The method is used to process PIM Join Timer. It is timer for (S,G) and (*,G).
 * When Join Timer expires, periodic Join message is sent to upstream neighbor.
 *
 * @param timer Pointer to Join Timer.
 * @see PIMjt()
 */
void pimSM::processJoinTimer(PIMjt *timer)
{
    EV << "pimSM::processJoinTimer:" << endl;

    AnsaIPv4MulticastRoute *route;

    if (timer->getJoinType() == SG)
    {
        route = rt->getRouteFor(timer->getGroup(), timer->getJoinPruneAddr());
        sendPIMJoinPrune(timer->getGroup(),timer->getJoinPruneAddr(),timer->getUpstreamNbr(),JoinMsg,SG);
    }
    else if (timer->getJoinType() == G)
    {
        route = rt->getRouteFor(timer->getGroup(), IPv4Address::UNSPECIFIED_ADDRESS);
        sendPIMJoinPrune(timer->getGroup(),timer->getJoinPruneAddr(),timer->getUpstreamNbr(),JoinMsg,G);
    }
    else
        throw cRuntimeError("pimSM::processJoinTimer - Bad Join type!");

    // restart JT timer
    if (route->getJt())
    {
        cancelEvent(route->getJt());
        scheduleAt(simTime() + JT, route->getJt());
    }
}

/**
 * PROCESS PRUNE PENDING TIMER
 *
 * The method is used to process PIM Prune Pending Timer.
 * Prune Pending Timer is used for delaying of Prune message sending
 * (for possible overriding Join from another PIM neighbor)
 *
 * @param timer Pointer to Prune Pending Timer.
 * @see PIMppt()
 */
void pimSM::processPrunePendingTimer(PIMppt *timer)
{
    EV << "pimSM::processPrunePendingTimer:" << endl;

    if (!IamRP(this->getRPAddress()))
        sendPIMJoinPrune(timer->getGroup(),timer->getJoinPruneAddr(),timer->getUpstreamNbr(),PruneMsg,G);

    AnsaIPv4MulticastRoute *route = rt->getRouteFor(timer->getGroup(), IPv4Address::UNSPECIFIED_ADDRESS);
    if (route->getPpt())
        route->setPpt(NULL);
    delete timer;
}

/**
 * PROCESS PIM TIMER
 *
 * The method is used to process PIM timers. According to type of PIM timer, the timer is sent to
 * appropriate method.
 *
 * @param timer Pointer to PIM timer.
 * @see PIMTimer()
 * @see processPruneTimer()
 * @see processGraftRetryTimer()
 */
void pimSM::processPIMTimer(PIMTimer *timer)
{
    EV << "pimSM::processPIMTimer: ";

    switch(timer->getTimerKind())
    {
        case JoinTimer:
            EV << "JoinTimer" << endl;
            processJoinTimer(check_and_cast<PIMjt *> (timer));
            break;
        case PrunePendingTimer:
            EV << "PrunePendingTimer" << endl;
            processPrunePendingTimer(check_and_cast<PIMppt *> (timer));
            break;
        case ExpiryTimer:
            EV << "ExpiryTimer" << endl;
            processExpiryTimer(check_and_cast<PIMet *> (timer));
            break;
        case KeepAliveTimer:
            EV << "KeepAliveTimer" << endl;
            processKeepAliveTimer(check_and_cast<PIMkat *> (timer));
            break;
        case RegisterStopTimer:
            EV << "RegisterStopTimer" << endl;
            processRegisterStopTimer(check_and_cast<PIMrst *> (timer));
            break;
        default:
            EV << "BAD TYPE, DROPPED" << endl;
            delete timer;
            break;
    }
}


void pimSM::processJoinPrunePacket(PIMJoinPrune *pkt)
{
    EV <<  "pimSM::processJoinPrunePacket" << endl;
    EncodedAddress encodedAddr;

    for (unsigned int i = 0; i < pkt->getMulticastGroupsArraySize(); i++)
    {
        MulticastGroup group = pkt->getMulticastGroups(i);
        IPv4Address multGroup = group.getGroupAddress();

        if (group.getJoinedSourceAddressArraySize() > 0)
        {
            AnsaIPv4MulticastRoute *routeG = rt->getRouteFor(multGroup,IPv4Address::UNSPECIFIED_ADDRESS);
            if (routeG)
            {
                if (routeG->getPpt())
                {
                    cancelEvent(routeG->getPpt());
                    delete routeG->getPpt();
                }
            }
            encodedAddr = group.getJoinedSourceAddress(0);
            processJoinPacket(pkt,multGroup,encodedAddr);
        }
        if (group.getPrunedSourceAddressArraySize())
        {
            encodedAddr = group.getPrunedSourceAddress(0);
            processPrunePacket(pkt,multGroup,encodedAddr);
        }
    }
}

void pimSM::processPrunePacket(PIMJoinPrune *pkt, IPv4Address multGroup, EncodedAddress encodedAddr)
{
    EV <<  "pimSM::processPrunePacket" << endl;

    vector<AnsaIPv4MulticastRoute*> routes = rt->getRouteFor(multGroup);
    IPv4ControlInfo *ctrl = (IPv4ControlInfo *) pkt->getControlInfo();
    int outIntToDel = ctrl->getInterfaceId();

    // go through all multicast routes
    for (unsigned int j = 0; j < routes.size(); j++)
    {
        AnsaIPv4MulticastRoute *route = routes[j];
        InterfaceVector outInt = route->getOutInt();
        unsigned int k;

        // is interface in list of outgoing interfaces?
        for (k = 0; k < outInt.size(); k++)
        {
            if (outInt[k].intId == outIntToDel)
            {
                EV << "Interface is present, removing it from the list of outgoing interfaces." << endl;
                outInt.erase(outInt.begin() + k);
            }
        }
        route->setOutInt(outInt);

        // there is no receiver of multicast, prune the router from the multicast tree
        if (route->isOilistNull())
        {
            route->removeFlag(C);
            route->addFlag(P);
            if (!IamRP(this->getRPAddress()))                                                                               // only for non-RP routers in RPT
            {
                PimNeighbor *neighborToRP = pimNbt->getNeighborByIntID(route->getInIntId());
                PIMppt* timerPppt = createPrunePendingTimer(multGroup,this->getRPAddress(),neighborToRP->getAddr());
                if (route->getOrigin() == IPv4Address::UNSPECIFIED_ADDRESS && !route->getPpt())                             // only for (*,G) route and if there isn't active PPT
                    route->setPpt(timerPppt);                                                                               // "Send" delayed Prune toward PR
                if (route->getJt())                                                                                         // if is JT set, delete it
                {
                    cancelEvent(route->getJt());
                    delete route->getJt();
                    route->setJt(NULL);
                }
            }
        }
    }
    rt->generateShowIPMroute();
}

void pimSM::processSGJoin(PIMJoinPrune *pkt,IPv4Address multOrigin, IPv4Address multGroup)
{
    AnsaIPv4MulticastRoute *newRouteSG = new AnsaIPv4MulticastRoute();
    InterfaceEntry *newInIntSG = rt->getInterfaceForDestAddr(multOrigin);
    PimNeighbor *neighborToSrcDR = pimNbt->getNeighborByIntID(newInIntSG->getInterfaceId());
    AnsaIPv4MulticastRoute *routePointer;

    std::cout << "src: " << multOrigin << "dst: " << multGroup << endl;
    routePointer = newRouteSG;
    if (!(newRouteSG = rt->getRouteFor(multGroup, multOrigin)))
    {
        newRouteSG = routePointer;
        // set source, mult. group, etc...
        newRouteSG->setAddresses(multOrigin,multGroup,this->getRPAddress());
        // set outgoing and incoming interface
        InterfaceEntry *interface = rt->getInterfaceForDestAddr(this->getRPAddress());
        newRouteSG->setInInt(newInIntSG, newInIntSG->getInterfaceId(), neighborToSrcDR->getAddr());
        newRouteSG->addOutIntFull(interface,interface->getInterfaceId(),Forward,Sparsemode,NULL,NULL,NoInfo,NoInfoRS);

        // create and set (S,G) ET and JT timers
        //FIXME ET for SG?
        //newRouteSG->setEt(createExpiryTimer(pkt->getHoldTime(), multGroup));
        if (!IamDR(multOrigin))
            newRouteSG->setJt(createJoinTimer(multGroup, multOrigin, neighborToSrcDR->getAddr(),SG));

        rt->addMulticastRoute(newRouteSG);

        if (!IamDR(multOrigin))
            sendPIMJoinPrune(multGroup,multOrigin,neighborToSrcDR->getAddr(),JoinMsg,SG);       // triggered join except DR
    }
    else
    {
        if (IamDR(multOrigin))
        {
            InterfaceVector outIntVect = newRouteSG->getOutInt();
            newRouteSG->removeFlag(P);
            for (unsigned j=0; j < outIntVect.size(); j++)                                                                      // update interfaces to forwarding state
                outIntVect[j].forwarding = Forward;
            newRouteSG->setOutInt(outIntVect);
            newRouteSG->setOutShowIntStatus(true);
        }
    }

    if (!IamDR(multOrigin))
    {
        AnsaIPv4MulticastRoute *newRouteG = new AnsaIPv4MulticastRoute();
        routePointer = newRouteG;
        if (!(newRouteG = rt->getRouteFor(multGroup, IPv4Address::UNSPECIFIED_ADDRESS)))
        {
            InterfaceEntry *newInIntG = rt->getInterfaceForDestAddr(this->RPAddress);
            PimNeighbor *neighborToRP = pimNbt->getNeighborByIntID(newInIntG->getInterfaceId());

            newRouteG = routePointer;
            newRouteG->setAddresses(IPv4Address::UNSPECIFIED_ADDRESS,multGroup,this->getRPAddress());
            newRouteG->addFlags(S,P,NO_FLAG,NO_FLAG);
            newRouteG->setInInt(newInIntG, newInIntG->getInterfaceId(), neighborToRP->getAddr());
            rt->addMulticastRoute(newRouteG);
        }
    }
    rt->generateShowIPMroute();
}

void pimSM::processJoinPacket(PIMJoinPrune *pkt, IPv4Address multGroup, EncodedAddress encodedAddr)
{
    IPv4ControlInfo *ctrl = (IPv4ControlInfo *) pkt->getControlInfo();
    AnsaIPv4MulticastRoute *newRouteG = new AnsaIPv4MulticastRoute();
    AnsaIPv4MulticastRoute *routePointer;
    IPv4Address multOrigin;

    if (!encodedAddr.R && !encodedAddr.W && encodedAddr.S)                                              // (S,G) Join
    {
        multOrigin = encodedAddr.IPaddress;
        processSGJoin(pkt,multOrigin,multGroup);
    }
    if (encodedAddr.R && encodedAddr.W && encodedAddr.S)                                                // (*,G) Join
    {
        routePointer = newRouteG;
        if (!(newRouteG = rt->getRouteFor(multGroup, IPv4Address::UNSPECIFIED_ADDRESS)))                                // check if (*,G) exist
        {
            newRouteG = routePointer;
            InterfaceEntry *newInIntG = rt->getInterfaceForDestAddr(this->RPAddress);
            PimNeighbor *neighborToRP = pimNbt->getNeighborByIntID(newInIntG->getInterfaceId());

            newRouteG->setAddresses(IPv4Address::UNSPECIFIED_ADDRESS,multGroup,this->getRPAddress());                       // set source, mult. group, etc...
            newRouteG->addFlag(S);
            if (!IamRP(this->getRPAddress()))                                                                               //  (*,G) route hasn't incoming interface at RP
                newRouteG->setInInt(newInIntG, newInIntG->getInterfaceId(), neighborToRP->getAddr());                           // set incoming interface
            InterfaceEntry *interface = rt->getInterfaceForDestAddr(ctrl->getSrcAddr());

            newRouteG->addOutIntFull(interface,interface->getInterfaceId(),Forward,Sparsemode,NULL,NULL,NoInfo,NoInfoRS);        // set outgoing interface
            newRouteG->setEt(createExpiryTimer(pkt->getHoldTime(), multGroup,IPv4Address::UNSPECIFIED_ADDRESS,G));          // set Timers
            newRouteG->setJt(createJoinTimer(multGroup, this->getRPAddress(), neighborToRP->getAddr(),G));
            rt->addMulticastRoute(newRouteG);

            if (!IamRP(this->getRPAddress()))
                sendPIMJoinPrune(multGroup,this->getRPAddress(),neighborToRP->getAddr(),JoinMsg,G);                         // triggered Join (*,G)
        }
        else                                                                                                            // (*,G) route exist
        {
            if (IamRP(this->getRPAddress()))
            {
                std::vector<AnsaIPv4MulticastRoute*> routes = rt->getRouteFor(multGroup);                               // get all mult. routes at RP
                for (unsigned i=0; i<routes.size();i++)
                {
                    routePointer = routes[i];
                    multOrigin = routePointer->getOrigin();
                    if (routePointer->getOrigin() != IPv4Address::UNSPECIFIED_ADDRESS)                                         // for (S,G) route
                    {
                        routePointer->removeFlag(P);                                                                                // update flags
                        routePointer->addFlag(T);
                        routePointer->setJt(createJoinTimer(multGroup, multOrigin, routePointer->getInIntNextHop(),SG));               // set JT

                        InterfaceVector outIntVect = routePointer->getOutInt();
                        if (outIntVect.size() == 0)                                                                                         // Has route any outgoing interface?
                        {                                                                                                                       // any interface available
                            InterfaceEntry *interface = rt->getInterfaceForDestAddr(ctrl->getSrcAddr());
                            routePointer->addOutIntFull(interface,interface->getInterfaceId(),Forward,Sparsemode,NULL,NULL,NoInfo,NoInfoRS);         // add outgoing interface for (S,G)
                            sendPIMJoinPrune(multGroup, multOrigin, routePointer->getInIntNextHop(),JoinMsg,SG);
                        }
                    }
                    else                                                                                                // for (*,G) route
                    {
                        if (routePointer->isFlagSet(P) || routePointer->isFlagSet(F))
                        {
                            routePointer->removeFlag(P);
                            routePointer->removeFlag(F);
                        }
                        InterfaceEntry *interface = rt->getInterfaceForDestAddr(ctrl->getSrcAddr());
                        if (!routePointer->outIntExist(interface->getInterfaceId()))
                            routePointer->addOutIntFull(interface,interface->getInterfaceId(),Forward,Sparsemode,NULL,NULL,NoInfo,NoInfoRS);
                    }
                }
            }
            else                                                                                                        // (*,G) route exist somewhere in RPT
            {
                InterfaceEntry *interface = rt->getInterfaceForDestAddr(ctrl->getSrcAddr());
                if (!newRouteG->outIntExist(interface->getInterfaceId()))
                    newRouteG->addOutIntFull(interface,interface->getInterfaceId(),Forward,Sparsemode,NULL,NULL,NoInfo,NoInfoRS);    // add outgoing interface for (*,G)
            }
            if (newRouteG->getEt())                                                                                     // restart ET timer
            {
                EV << " (*,G) ET timer refresh" << endl;
                cancelEvent(newRouteG->getEt());
                scheduleAt(simTime() + pkt->getHoldTime(), newRouteG->getEt());
            }
        }
    }
    rt->generateShowIPMroute();
}

void pimSM::processRegisterPacket(PIMRegister *pkt)
{
    EV << "pimSM:processRegisterPacket" << endl;

    AnsaIPv4MulticastRoute *newRouteG = new AnsaIPv4MulticastRoute();
    AnsaIPv4MulticastRoute *newRoute = new AnsaIPv4MulticastRoute();
    AnsaIPv4MulticastRoute *routePointer;
    MultData *encapData = &(pkt->getEncapsulatedData());
    IPv4Address multOrigin = encapData->getMultOrigin();
    IPv4Address multGroup = encapData->getMultGroup();
    multDataInfo *info = new multDataInfo;

    if (!pkt->getN())                                                                                       //It is Null Register ?
    {
        routePointer = newRouteG;
        if (!(newRouteG = rt->getRouteFor(multGroup, IPv4Address::UNSPECIFIED_ADDRESS)))                    // check if exist (*,G)
        {
            newRouteG = routePointer;
            newRouteG->setAddresses(IPv4Address::UNSPECIFIED_ADDRESS,multGroup,this->getRPAddress());
            newRouteG->addFlags(S,P,NO_FLAG,NO_FLAG);                                                       // create and set (*,G) KAT timer, add to routing table
            newRouteG->setKat(createKeepAliveTimer(IPv4Address::UNSPECIFIED_ADDRESS, newRouteG->getMulticastGroup()));
            rt->addMulticastRoute(newRouteG);
        }
        routePointer = newRoute;                                                                            // check if exist (S,G)
        if (!(newRoute = rt->getRouteFor(multGroup,multOrigin)))
        {
            InterfaceEntry *newInIntG = rt->getInterfaceForDestAddr(multOrigin);
            PimNeighbor *pimIntfToDR = pimNbt->getNeighborByIntID(newInIntG->getInterfaceId());
            newRoute = routePointer;
            newRoute->setInInt(newInIntG, newInIntG->getInterfaceId(), pimIntfToDR->getAddr());
            newRoute->setAddresses(multOrigin,multGroup,this->getRPAddress());
            newRoute->addFlag(P);
            newRoute->setKat(createKeepAliveTimer(newRoute->getOrigin(), newRoute->getMulticastGroup()));   // create and set (S,G) KAT timer, add to routing table
            rt->addMulticastRoute(newRoute);
        }
                                                                                                            // we have some active receivers
        if (!newRouteG->isOilistNull())
        {
            newRoute->setOutInt(newRouteG->getOutInt());
            newRoute->removeFlag(P);                                                                        // update flags for SG route

            if (!newRoute->isFlagSet(T))                                                                    // only if isn't build SPT between RP and registering DR
            {
                InterfaceVector outVect = newRouteG->getOutInt();
                for (unsigned i=0; i < outVect.size(); i++)
                {
                    if (outVect[i].forwarding == Forward)                                                   // for active outgoing interface forward encapsulated data
                    {                                                                                       // simulate multicast data
                        info->group = multGroup;
                        info->origin = multOrigin;
                        info->interface_id = outVect[i].intId;
                        InterfaceEntry *entry = ift->getInterfaceById(outVect[i].intId);
                        info->srcAddr = entry->ipv4Data()->getIPAddress();
                        forwardMulticastData(info);
                    }
                }
                sendPIMJoinTowardSource(info);                                                              // send Join(S,G) to establish SPT between RP and registering DR
                IPv4ControlInfo *PIMctrl =  (IPv4ControlInfo *) pkt->getControlInfo();                      // send register-stop packet
                sendPIMRegisterStop(PIMctrl->getDestAddr(),PIMctrl->getSrcAddr(),multGroup,multOrigin);
            }
        }
        if (newRoute->getKat())                                                                             // refresh KAT timers
        {
            EV << " (S,G) KAT timer refresh" << endl;
            cancelEvent(newRoute->getKat());
            scheduleAt(simTime() + KAT, newRoute->getKat());
        }
        if (newRouteG->getKat())
        {
            EV << " (*,G) KAT timer refresh" << endl;
            cancelEvent(newRouteG->getKat());
            scheduleAt(simTime() + 2*KAT, newRouteG->getKat());
        }
    }
    else
    {                                                                                                       //get routes for next step if register is register-null
        newRoute =  rt->getRouteFor(multGroup,multOrigin);
        newRouteG = rt->getRouteFor(multGroup, IPv4Address::UNSPECIFIED_ADDRESS);
    }

    if ((newRoute->isFlagSet(P) && newRouteG->isFlagSet(P)) || pkt->getN())
    {                                                                                                       // send register-stop packet
        IPv4ControlInfo *PIMctrl =  (IPv4ControlInfo *) pkt->getControlInfo();
        sendPIMRegisterStop(PIMctrl->getDestAddr(),PIMctrl->getSrcAddr(),multGroup,multOrigin);
    }
    rt->generateShowIPMroute();
}

void pimSM::processRegisterStopPacket(PIMRegisterStop *pkt)
{
    EV << "pimSM:processRegisterStopPacket" << endl;

    AnsaIPv4MulticastRoute *routeSG = new AnsaIPv4MulticastRoute();
    InterfaceEntry *intToRP = rt->getInterfaceForDestAddr(this->getRPAddress());
    int intToRPId;

    // Set RST timer for S,G route
    PIMrst* timerRST = createRegisterStopTimer(pkt->getSourceAddress(), pkt->getGroupAddress());
    routeSG = rt->getRouteFor(pkt->getGroupAddress(),pkt->getSourceAddress());
    routeSG->setRst(timerRST);

    if (routeSG == NULL)
        throw cRuntimeError("pimSM::processRegisterStopPacket - route for (S,G) not found!");

    intToRPId = intToRP->getInterfaceId();
    if (routeSG->getRegStatus(intToRPId) == Join)
    {
        EV << "Register tunnel is connect - has to be disconnect" << endl;
        routeSG->setRegStatus(intToRPId,Prune);
    }
    else
        EV << "Register tunnel is disconnect" << endl;

}

/**
 * PROCESS PIM PACKET
 *
 * The method is used to process PIM packets. According to type of PIM packet, the packet is sent to
 * appropriate method.
 *
 * @param pkt Pointer to PIM packet.
 * @see PIMPacket()
 */
void pimSM::processPIMPkt(PIMPacket *pkt)
{
    EV << "pimSM::processPIMPkt: ";

    switch(pkt->getType())
    {
        case JoinPrune:
            EV << "JoinPrune" << endl;
            processJoinPrunePacket(check_and_cast<PIMJoinPrune *> (pkt));
            break;
        case Register:
            EV << "Register" << endl;
            processRegisterPacket(check_and_cast<PIMRegister *> (pkt));
            break;
        case RegisterStop:
            EV << "Register-stop" << endl;
            processRegisterStopPacket(check_and_cast<PIMRegisterStop *> (pkt));
            break;
        case Assert:
            EV << "Assert" << endl;
            // TODO for future use
            break;
        default:
            EV << "BAD TYPE, DROPPED" << endl;
            delete pkt;
            break;
    }
}

void pimSM::sendPIMJoinPrune(IPv4Address multGroup, IPv4Address joinPruneIPaddr, IPv4Address upstreamNbr, joinPruneMsg msgType, JPMsgType JPtype)
{
    // create PIM Register datagram
    PIMJoinPrune *msg = new PIMJoinPrune();
    MulticastGroup *group = new MulticastGroup();
    EncodedAddress encodedAddr;

    // set PIM packet
    msg->setName("PIMJoin/Prune");
    msg->setType(JoinPrune);
    msg->setUpstreamNeighborAddress(upstreamNbr);
    msg->setHoldTime(HOLDTIME);

    encodedAddr.IPaddress = joinPruneIPaddr;
    if (JPtype == G)
    {
        EV << "pimSM::sendPIMJoinPrune, assembling (*,G) ";
        encodedAddr.S = true;
        encodedAddr.W = true;
        encodedAddr.R = true;
    }
    if (JPtype == SG)
    {
        EV << "pimSM::sendPIMJoinPrune, assembling (S,G) ";
        encodedAddr.S = true;
        encodedAddr.W = false;
        encodedAddr.R = false;
    }

    msg->setMulticastGroupsArraySize(1);
    group->setGroupAddress(multGroup);
    if (msgType == JoinMsg)
    {
        EV << "Join" << endl;
        group->setJoinedSourceAddressArraySize(1);
        group->setPrunedSourceAddressArraySize(0);
        group->setJoinedSourceAddress(0,encodedAddr);
    }
    if (msgType == PruneMsg)
    {
        EV << "Prune" << endl;
        group->setJoinedSourceAddressArraySize(0);
        group->setPrunedSourceAddressArraySize(1);
        EncodedAddress PruneAddress;
        group->setPrunedSourceAddress(0,encodedAddr);
    }
    msg->setMulticastGroups(0, *group);
    // set IP Control info
    InterfaceEntry *interfaceToRP = rt->getInterfaceForDestAddr(joinPruneIPaddr);
    IPv4ControlInfo *ctrl = setCtrlForMessage(IPv4Address(ALL_PIM_ROUTERS),interfaceToRP->ipv4Data()->getIPAddress(),
                                                        IP_PROT_PIM,interfaceToRP->getInterfaceId(),1);
    msg->setControlInfo(ctrl);
    send(msg, "spiltterOut");
}

void pimSM::sendPIMRegisterNull(IPv4Address multSource, IPv4Address multDest)
{
    EV << "pimSM::sendPIMRegisterNull" << endl;

    // create PIM Register NULL datagram
    PIMRegister *msg = new PIMRegister();
    MultData *encapData = new MultData();

    // only if (S,G exist)
    if (rt->getRouteFor(multDest,multSource))
    {
        // set fields for PIM Register packet
        msg->setName("PIMRegisterNull");
        msg->setType(Register);
        msg->setN(true);
        msg->setB(false);

        // set encapsulated packet (MultData simulate multicast packet)
        encapData->setMultGroup(multDest);
        encapData->setMultOrigin(multSource);
        msg->setEncapsulatedData(*encapData);

        // set IP Control info
        InterfaceEntry *interfaceToRP = rt->getInterfaceForDestAddr(RPAddress);
        IPv4ControlInfo *ctrl = setCtrlForMessage(RPAddress,interfaceToRP->ipv4Data()->getIPAddress(),
                                                            IP_PROT_PIM,interfaceToRP->getInterfaceId(),MAX_TTL);
        msg->setControlInfo(ctrl);
        send(msg, "spiltterOut");
    }
}

void pimSM::sendPIMRegister(IPv4ControlInfo *ctrl)
{
    EV << "pimSM::sendPIMRegister" << endl;

    IPv4Address multGroup = ctrl->getDestAddr();
    IPv4Address multOrigin = ctrl->getSrcAddr();
    AnsaIPv4MulticastRoute *routeSG = new AnsaIPv4MulticastRoute();
    AnsaIPv4MulticastRoute *routeG = new AnsaIPv4MulticastRoute();
    InterfaceEntry *intToRP = rt->getInterfaceForDestAddr(this->getRPAddress());

    routeSG = rt->getRouteFor(multGroup,multOrigin);
    routeG = rt->getRouteFor(multGroup, IPv4Address::UNSPECIFIED_ADDRESS);
    if (routeSG == NULL)
        throw cRuntimeError("pimSM::sendPIMRegister - route for (S,G) not found!");

    // refresh KAT timers
    if (routeSG->getKat())
    {
        EV << " (S,G) KAT timer refresh" << endl;
        cancelEvent(routeSG->getKat());
        scheduleAt(simTime() + KAT, routeSG->getKat());
    }
    if (routeG->getKat())
    {
        EV << " (*,G) KAT timer refresh" << endl;
        cancelEvent(routeG->getKat());
        scheduleAt(simTime() + 2*KAT, routeG->getKat());
    }

    // Check if is register tunnel connected
    if (routeSG->getRegStatus(intToRP->getInterfaceId()) == Join)
    {
        // create PIM Register datagram
        PIMRegister *msg = new PIMRegister();
        MultData *encapData = new MultData();
        encapData->setMultGroup(multGroup);
        encapData->setMultOrigin(multOrigin);

        // set fields for PIM Register packet
        msg->setName("PIMRegister");
        msg->setType(Register);
        msg->setN(false);
        msg->setB(false);
        msg->setEncapsulatedData(*encapData);

        // set IP Control info
        InterfaceEntry *interfaceToRP = rt->getInterfaceForDestAddr(RPAddress);
        IPv4ControlInfo *ctrl = setCtrlForMessage(RPAddress,interfaceToRP->ipv4Data()->getIPAddress(),
                                                    IP_PROT_PIM,interfaceToRP->getInterfaceId(),MAX_TTL);
        msg->setControlInfo(ctrl);
        send(msg, "spiltterOut");
    }
    else if (routeSG->getRegStatus(intToRP->getInterfaceId()) == Prune)
        EV << "PIM-SM:sendPIMRegister - register tunnel is disconnect." << endl;
}

void pimSM::sendPIMJoinTowardSource(multDataInfo *info)
{
    EV << "pimSM::sendPIMJoinTowardSource" << endl;

    AnsaIPv4MulticastRoute *routeSG = rt->getRouteFor(info->group,info->origin);
    IPv4Address rpfNBR = routeSG->getInIntNextHop();
    sendPIMJoinPrune(info->group,info->origin, rpfNBR,JoinMsg,SG);
}

void pimSM::sendPIMRegisterStop(IPv4Address source, IPv4Address dest, IPv4Address multGroup, IPv4Address multSource)
{
    EV << "pimSM::sendPIMRegisterStop" << endl;

    // create PIM Register datagram
    PIMRegisterStop *msg = new PIMRegisterStop();

    // set PIM packet
    msg->setName("PIMRegisterStop");
    msg->setType(RegisterStop);
    msg->setSourceAddress(multSource);
    msg->setGroupAddress(multGroup);

    // set IP packet
    InterfaceEntry *interfaceToDR = rt->getInterfaceForDestAddr(dest);
    IPv4ControlInfo *ctrl = new IPv4ControlInfo();
    ctrl->setDestAddr(dest);
    ctrl->setSrcAddr(source);
    ctrl->setProtocol(IP_PROT_PIM);
    ctrl->setInterfaceId(interfaceToDR->getInterfaceId());
    ctrl->setTimeToLive(255);
    msg->setControlInfo(ctrl);

    send(msg, "spiltterOut");
}


void pimSM::forwardMulticastData(multDataInfo *info)
{
    EV << "pimSM::forwardMulticastData" << endl;

    MultData *data = new MultData();
    std::stringstream os;
    std::string dataName;

    // set informations about fictive multicast packet
    os << "MultData " << info->group << endl;
    dataName.append(os.str());
    data->setName(dataName.c_str());
    data->setMultOrigin(info->origin);
    data->setMultGroup(info->group);

    // set control info
    IPv4ControlInfo *ctrl = new IPv4ControlInfo();
    ctrl->setDestAddr(info->group);
    ctrl->setSrcAddr(info->srcAddr);
    ctrl->setInterfaceId(info->interface_id);
    ctrl->setTimeToLive(MAX_TTL-2);                     //one minus for source DR router and one for RP router
    data->setControlInfo(ctrl);
    send(data, "spiltterOut");
}

/**
 * NEW MULTICAST
 *
 * The method process notification about new multicast data stream.
 *
 * @param newRoute Pointer to new entry in the multicast routing table.
 * @see
 */
void pimSM::newMulticastRegisterDR(AnsaIPv4MulticastRoute *newRoute)
{
    EV << "pimSM::newMulticastRegisterDR" << endl;

    AnsaIPv4MulticastRoute *newRouteG = new AnsaIPv4MulticastRoute();

    // Set Keep Alive timer for routes
    PIMkat* timerKat = createKeepAliveTimer(newRoute->getOrigin(), newRoute->getMulticastGroup());
    PIMkat* timerKatG = createKeepAliveTimer(IPv4Address::UNSPECIFIED_ADDRESS, newRoute->getMulticastGroup());
    newRoute->setKat(timerKat);
    newRouteG->setKat(timerKatG);

    //TODO Could register?

    //Create (*,G) state
    InterfaceEntry *newInIntG = rt->getInterfaceForDestAddr(this->getRPAddress());
    newRouteG->setInInt(newInIntG, newInIntG->getInterfaceId(), this->getRPAddress());
    newRouteG->setAddresses(IPv4Address::UNSPECIFIED_ADDRESS,newRoute->getMulticastGroup(),this->getRPAddress());
    newRouteG->addFlags(S,P,F,NO_FLAG);

    //Create (S,G) state - set flags and Register state, other is set by  PimSplitter
    newRoute->addFlags(P,F,T,NO_FLAG);
    newRoute->addOutIntFull(newInIntG,newInIntG->getInterfaceId(),Pruned,Sparsemode,NULL,NULL,NoInfo,Join);      // create new outgoing interface to RP
    newRoute->setRP(this->getRPAddress());
    newRoute->setOutShowIntStatus(false);                   //we need to set register state to output interface, but output interface has to be null for now

    rt->addMulticastRoute(newRouteG);
    rt->addMulticastRoute(newRoute);

    EV << "pimSM::newMulticast: New routes was added to the multicast routing table." << endl;
}

void pimSM::removeMulticastReciever(addRemoveAddr *members)
{
    EV << "pimSM::removeMulticastReciever" << endl;

    std::vector<IPv4Address> remMembers = members->getAddr();
    PimInterface *pimInt = members->getInt();

    for (unsigned int i = 0; i < remMembers.size(); i++)
    {
        EV << "Removed multicast address: " << remMembers[i] << endl;
        vector<AnsaIPv4MulticastRoute*> routes = rt->getRouteFor(remMembers[i]);

        // there is no route for group in the table
        if (routes.size() == 0)
            continue;

        // go through all multicast routes
        for (unsigned int j = 0; j < routes.size(); j++)
        {
            AnsaIPv4MulticastRoute *route = routes[j];
            PimNeighbor *neighborToRP = pimNbt->getNeighborByIntID(route->getInIntId());
            InterfaceVector outInt = route->getOutInt();
            unsigned int k;

            // is interface in list of outgoing interfaces?
            for (k = 0; k < outInt.size(); k++)
            {
                if (outInt[k].intId == pimInt->getInterfaceID())
                {
                    EV << "Interface is present, removing it from the list of outgoing interfaces." << endl;
                    outInt.erase(outInt.begin() + k);
                }
            }
            route->setOutInt(outInt);
            route->removeFlag(C);
            // there is no receiver of multicast, prune the router from the multicast tree
            if (route->isOilistNull())
            {
                route->addFlag(P);
                sendPIMJoinPrune(route->getMulticastGroup(),this->getRPAddress(),neighborToRP->getAddr(),PruneMsg,G);
                if (route->getJt())
                {
                    cancelEvent(route->getJt());
                    delete route->getJt();
                    route->setJt(NULL);
                }
            }
        }
    }
    rt->generateShowIPMroute();
}

void pimSM::newMulticastReciever(addRemoveAddr *members)
{
    EV << "pimSM::newMulticastReciever" << endl;

    AnsaIPv4MulticastRoute *newRouteG = new AnsaIPv4MulticastRoute();
    AnsaIPv4MulticastRoute *routePointer;
    std::vector<IPv4Address> addMembers = members->getAddr();

    for (unsigned i=0; i<addMembers.size();i++)
    {
        IPv4Address multGroup = addMembers[i];
        int interfaceId = (members->getInt())->getInterfaceID();
        InterfaceEntry *newInIntG = rt->getInterfaceForDestAddr(this->RPAddress);
        PimNeighbor *neighborToRP = pimNbt->getNeighborByIntID(newInIntG->getInterfaceId());

        routePointer = newRouteG;
        if (!(newRouteG = rt->getRouteFor(multGroup,IPv4Address::UNSPECIFIED_ADDRESS)))                             // create new (*,G) route
        {
            newRouteG = routePointer;
            // set source, mult. group, etc...
            newRouteG->setAddresses(IPv4Address::UNSPECIFIED_ADDRESS,multGroup,this->getRPAddress());
            newRouteG->addFlags(S,C,NO_FLAG,NO_FLAG);

            // set incoming interface
            newRouteG->setInInt(newInIntG, newInIntG->getInterfaceId(), neighborToRP->getAddr());

            // set outgoing interface to RP
            InterfaceEntry *interface = ift->getInterfaceById(interfaceId);
            newRouteG->addOutIntFull(interface,interface->getInterfaceId(),Forward,Sparsemode,NULL,NULL,NoInfo,NoInfoRS);

            // create and set (*,G) ET timer
            PIMet* timerEt = createExpiryTimer(HOLDTIME,multGroup,IPv4Address::UNSPECIFIED_ADDRESS, G);
            PIMjt* timerJt = createJoinTimer(multGroup, this->getRPAddress(), neighborToRP->getAddr(),G);
            newRouteG->setEt(timerEt);
            newRouteG->setJt(timerJt);

            rt->addMulticastRoute(newRouteG);

            // oilist != NULL -> send Join(*,G) to 224.0.0.13
            if (!newRouteG->isOilistNull())
                sendPIMJoinPrune(multGroup,this->getRPAddress(),neighborToRP->getAddr(),JoinMsg,G);
        }
        else                                                                                                        // add new outgoing interface to existing (*,G) route
        {
            InterfaceEntry *interface = ift->getInterfaceById(interfaceId);
            newRouteG->addOutIntFull(interface,interface->getInterfaceId(),Forward,Sparsemode,NULL,NULL,NoInfo,NoInfoRS);
            newRouteG->addFlag(C);

            // restart ET timer
            if (newRouteG->getEt())
            {
                EV << " (*,G) ET timer refresh" << endl;
                cancelEvent(newRouteG->getEt());
                scheduleAt(simTime() + HOLDTIME, newRouteG->getEt());
            }
        }
    }
    rt->generateShowIPMroute();
}

/**
 * RECEIVE CHANGE NOTIFICATION
 *
 * The method from class Notification Board is used to catch its events.
 *
 * @param category Category of notification.
 * @param details Additional information for notification.
 * @see newMulticast()
 * @see newMulticastAddr()
 */
void pimSM::receiveChangeNotification(int category, const cPolymorphic *details)
{
    // ignore notifications during initialize
    if (simulation.getContextType()==CTX_INITIALIZE)
        return;

    // PIM needs addition info for each notification
    if (details == NULL)
        return;

    Enter_Method_Silent();
    printNotificationBanner(category, details);
    AnsaIPv4MulticastRoute *route;
    IPv4ControlInfo *myCtrl;
    addRemoveAddr *members;

    // according to category of event...
    switch (category)
    {
        case NF_IPv4_NEW_IGMP_ADDED_PISM:
            EV <<  "pimSM::receiveChangeNotification - NEW IGMP ADDED" << endl;
            members = (addRemoveAddr *)(details);
            newMulticastReciever(members);
            break;

        case NF_IPv4_NEW_IGMP_REMOVED_PIMSM:
            EV <<  "pimSM::receiveChangeNotification - IGMP REMOVED" << endl;
            members = (addRemoveAddr *)(details);
            removeMulticastReciever(members);
            break;

        // new multicast data appears in router
        case NF_IPv4_NEW_MULTICAST_SPARSE:
            EV <<  "pimSM::receiveChangeNotification - NEW MULTICAST SPARSE" << endl;
            route = (AnsaIPv4MulticastRoute *)(details);
            newMulticastRegisterDR(route);
            break;

        // create PIM register packet
        case NF_IPv4_MDATA_REGISTER:
            EV <<  "pimSM::receiveChangeNotification - REGISTER DATA" << endl;
            myCtrl = (IPv4ControlInfo *)(details);
            sendPIMRegister(myCtrl);
            break;
    }
}
