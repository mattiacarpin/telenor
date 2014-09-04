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




	int numberOfNodes=50;
	double R_min=500;
	double R_max=2000;

	double simTime = 60.0;

	Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
	lteHelper->SetSpectrumChannelType ("ns3::MultiModelSpectrumChannel");
	lteHelper->SetSchedulerType ("ns3::TdMtFfMacScheduler");

	Config::SetDefault ("ns3::TcpSocket::SegmentSize",UintegerValue(1024));
	Config::SetDefault("ns3::LteAmc::AmcModel",EnumValue(LteAmc::PiroEW2010));		//Modello per il calcolo del CQI che valuta ciascun RB
	Config::SetDefault("ns3::LteEnbRrc::SrsPeriodicity",UintegerValue(160));		//Supporto per numerosi UEs
	Config::SetDefault("ns3::LteEnbPhy::TxPower",DoubleValue(30.0));				//Potenza trasmissiva (dBm) utilizzabile dagli eNode
	Config::SetDefault("ns3::LteUePhy::TxPower",DoubleValue(23.0));				//Potenza trasmissiva (dBm) utilizzabile dagli UEs
	Config::SetDefault ("ns3::LteSpectrumPhy::CtrlErrorModelEnabled", BooleanValue (false));
	Config::SetDefault ("ns3::LteSpectrumPhy::DataErrorModelEnabled", BooleanValue (false));
	Config::SetDefault ("ns3::LteUePhy::NoiseFigure", DoubleValue(5.0));
	Config::SetDefault ("ns3::LteEnbPhy::NoiseFigure", DoubleValue(5.0));


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

	// Now we are ready to setup the TCP application for each user


	uint16_t sinkPort = 10000;
	double initialLag=0.5;
	double arrivalPeriod=30;

	vector<double> arrivalTimes;

	for(int u=0;u<numberOfNodes;u++)
	{
		++sinkPort;
		Ptr<Node> ue = ueNodes.Get (u);

		//This app consumes data, stop!
		PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
		ApplicationContainer sinkApps = packetSinkHelper.Install (ue);
		sinkApps.Start (Seconds (initialLag));
		sinkApps.Stop (Seconds (simTime));

		//Setup the sender on the remote host
		BulkSendHelper source ("ns3::TcpSocketFactory", InetSocketAddress (ueIpIface.GetAddress (u), sinkPort));
		double packetSize=512;
		source.SetAttribute ("MaxBytes", UintegerValue (packetSize));
		ApplicationContainer sourceApps = source.Install (remoteHost);
		double packetArrivalTime=((double) rand()/(RAND_MAX))*(arrivalPeriod-initialLag);
		arrivalTimes.push_back(packetArrivalTime);
		sourceApps.Start (Seconds (initialLag+packetArrivalTime));
		sourceApps.Stop (Seconds (simTime));
	}

	sort(arrivalTimes.begin(),arrivalTimes.end());
	double firstTime=arrivalTimes[0];

	for (int i=0;i<numberOfNodes;i++)
		cout<<arrivalTimes[i]-firstTime<<endl;


	lteHelper->EnableTraces ();
	p2ph.EnablePcapAll("pcapTrace");

	Simulator::Stop(Seconds(simTime));
	Simulator::Run();
	Simulator::Destroy();
	return 0;
}


