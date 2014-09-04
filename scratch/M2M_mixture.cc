#include <ns3/core-module.h>
#include <ns3/network-module.h>
#include <ns3/mobility-module.h>
#include <ns3/internet-module.h>
#include <ns3/lte-module.h>
#include <ns3/config-store-module.h>
#include <ns3/buildings-module.h>
#include <ns3/point-to-point-helper.h>
#include <ns3/applications-module.h>
#include <ns3/log.h>
#include <iomanip>
#include <ios>
#include <string>
#include <vector>
#include <cmath>


using namespace ns3;
using namespace std;

int main (int argc, char *argv[])
{

	int RBs=100;
	int earfcn1=500;
	int numberOfNodes=10;
	double R_min=100;
	double R_max=200;

	double simTime = 10.0;

	Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
	lteHelper->SetSpectrumChannelType ("ns3::MultiModelSpectrumChannel");
	lteHelper->SetSchedulerType ("ns3::FdMtFfMacScheduler");

	Config::SetDefault ("ns3::TcpSocket::SegmentSize",UintegerValue(1024));
	Config::SetDefault("ns3::LteAmc::AmcModel",EnumValue(LteAmc::PiroEW2010));		//Modello per il calcolo del CQI che valuta ciascun RB
	Config::SetDefault("ns3::LteEnbRrc::SrsPeriodicity",UintegerValue(160));		//Supporto per numerosi UEs
	Config::SetDefault("ns3::LteEnbPhy::TxPower",DoubleValue(30.0));				//Potenza trasmissiva (dBm) utilizzabile dagli eNode
	Config::SetDefault("ns3::LteUePhy::TxPower",DoubleValue(23.0));				//Potenza trasmissiva (dBm) utilizzabile dagli UEs
	Config::SetDefault ("ns3::LteSpectrumPhy::CtrlErrorModelEnabled", BooleanValue (false));
	Config::SetDefault ("ns3::LteSpectrumPhy::DataErrorModelEnabled", BooleanValue (false));
	Config::SetDefault ("ns3::LteUePhy::NoiseFigure", DoubleValue(5.0));
	Config::SetDefault ("ns3::LteEnbPhy::NoiseFigure", DoubleValue(5.0));

	lteHelper->SetSpectrumChannelType ("ns3::MultiModelSpectrumChannel");
	lteHelper->SetEnbDeviceAttribute("DlBandwidth",UintegerValue(RBs));
	lteHelper->SetEnbDeviceAttribute("DlEarfcn",UintegerValue(earfcn1));
	lteHelper->SetEnbDeviceAttribute("UlEarfcn",UintegerValue(earfcn1+18000));

	Ptr<PointToPointEpcHelper>  epcHelper = CreateObject<PointToPointEpcHelper> ();
	lteHelper->SetEpcHelper (epcHelper);
	Ptr<Node> pgw = epcHelper->GetPgwNode ();

	NodeContainer remoteHostContainer;
	remoteHostContainer.Create (1);
	Ptr<Node> remoteHost = remoteHostContainer.Get (0);
	InternetStackHelper internet;
	internet.Install (remoteHostContainer);

	// Create the Internet
	PointToPointHelper p2ph;
	p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("20Mb/s")));
	p2ph.SetDeviceAttribute ("Mtu", UintegerValue (1500));
	p2ph.SetChannelAttribute ("Delay", TimeValue (Seconds (0.001)));

	NetDeviceContainer internetDevices = p2ph.Install (pgw, remoteHost);
	Ipv4AddressHelper ipv4h;
	ipv4h.SetBase ("1.0.0.0", "255.0.0.0");
	Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign (internetDevices);
	Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress (1);

	Ipv4StaticRoutingHelper ipv4RoutingHelper;
	Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> ());
	remoteHostStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 1);


	NodeContainer ueNodes;
	NodeContainer enbNodes;
	enbNodes.Create(1);
	ueNodes.Create(numberOfNodes);


	Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();

	/*Now we will deploy users in a circular corona of radii r_min and r_max*/
	for (int i=0;i<numberOfNodes;i++)
	{
		double A = ((double) rand() / (RAND_MAX));
		double phase=((double) rand() / (RAND_MAX))*2*3.14159;
		double d=sqrt(A*(R_max*R_max-R_min*R_min)+R_min*R_min);

		double x=d*cos(phase);
		double y=d*sin(phase);
		positionAlloc->Add(Vector(x, y, 0));
	}

	MobilityHelper mobility;
	mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
	mobility.SetPositionAllocator(positionAlloc);
	mobility.Install(ueNodes);

	MobilityHelper mobility2;
	mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
	Ptr<ListPositionAllocator> positionAlloc2 = CreateObject<ListPositionAllocator> ();
	positionAlloc2->Add (Vector(0, 0, 0));
	mobility2.SetPositionAllocator(positionAlloc2);
	mobility2.Install(enbNodes);

	// Install LTE Devices to the nodes
	NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice (enbNodes);
	NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice (ueNodes);

	// Install the IP stack on the UEs
	internet.Install (ueNodes);
	Ipv4InterfaceContainer ueIpIface;
	ueIpIface = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueLteDevs));
	// Assign IP address to UEs, and install applications
	for (uint32_t u = 0; u < ueNodes.GetN (); ++u)
	{
		Ptr<Node> ueNode = ueNodes.Get (u);
		Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (ueNode->GetObject<Ipv4> ());
		ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);
	}


	for (uint16_t i = 0; i < numberOfNodes; i++)
	{
		lteHelper->Attach (ueLteDevs.Get(i), enbLteDevs.Get(0));
	}

	//Now we should setup an UPLINK TCP connection for the first 5 users, and an UDP connection for user from 6...10
	int TCP_initial_port=65000;
	int UDP_initial_port=55000;

	for(int u=0;u<numberOfNodes;u++)
	{
		++TCP_initial_port;
		Ptr<Node> ue = ueNodes.Get (u);

		//Devo installare sul remoteHost un app e farla partire all'inizio della simulazione
		PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), TCP_initial_port));
		ApplicationContainer sinkApp = packetSinkHelper.Install (remoteHost);
		sinkApp.Start (Seconds (0.0));
		sinkApp.Stop (Seconds (simTime));

		//Now, setup the sender on the UE so we have generated the TCP connection
		BulkSendHelper source ("ns3::TcpSocketFactory", InetSocketAddress (remoteHostAddr, TCP_initial_port));
		double packetSize=1024;
		source.SetAttribute ("MaxBytes", UintegerValue (packetSize));
		ApplicationContainer sourceApps = source.Install (ue);

		double packetArrivalTime=((double) rand()/(RAND_MAX))*(simTime);
		cout<<packetArrivalTime<<endl;
		sourceApps.Start (Seconds (packetArrivalTime));
		sourceApps.Stop (Seconds (simTime));
	}

	int maxPacketSize=1500-32;

	for(int u=0;u<numberOfNodes;u++)
	{
		++UDP_initial_port;
		Ptr<Node> ue = ueNodes.Get (u);

		UdpClientHelper ulClientHelper (remoteHostAddr, UDP_initial_port);
		ulClientHelper.SetAttribute("MaxPackets",UintegerValue(200));
		ulClientHelper.SetAttribute("Interval", TimeValue (MilliSeconds (2)));
		ulClientHelper.SetAttribute("PacketSize",UintegerValue(maxPacketSize));



		ApplicationContainer clientApps;
		clientApps.Add (ulClientHelper.Install (ue));

		PacketSinkHelper ulPacketSinkHelper ("ns3::UdpSocketFactory",InetSocketAddress (Ipv4Address::GetAny (), UDP_initial_port));
		ApplicationContainer serverApps;
		serverApps.Add (ulPacketSinkHelper.Install (remoteHost));

		clientApps.Start (Seconds (1.5));
		clientApps.Stop (Seconds (simTime));

		serverApps.Start (Seconds (1.0));
		serverApps.Stop (Seconds (simTime));
	}

	lteHelper->EnableTraces ();
	p2ph.EnablePcapAll("pcapTrace");

	Simulator::Stop(Seconds(simTime));
	Simulator::Run();
	Simulator::Destroy();
	return 0;
}


