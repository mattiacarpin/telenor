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


using namespace ns3;


int main (int argc, char *argv[])
{
	//Single node that generates some TCP sessions according to a certain distribution

	double distance=1000;
	double simTime = 10.0;


	Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
	lteHelper->SetSpectrumChannelType ("ns3::MultiModelSpectrumChannel");

	Config::SetDefault("ns3::LteAmc::AmcModel",EnumValue(LteAmc::PiroEW2010));		//Modello per il calcolo del CQI che valuta ciascun RB
	Config::SetDefault("ns3::LteEnbRrc::SrsPeriodicity",UintegerValue(160));		//Supporto per numerosi UEs
	Config::SetDefault("ns3::LteEnbPhy::TxPower",DoubleValue(30.0));				//Potenza trasmissiva (dBm) utilizzabile dagli eNode
	Config::SetDefault("ns3::LteUePhy::TxPower",DoubleValue(23.0));				//Potenza trasmissiva (dBm) utilizzabile dagli UEs
	Config::SetDefault ("ns3::LteSpectrumPhy::CtrlErrorModelEnabled", BooleanValue (false));
	Config::SetDefault ("ns3::LteSpectrumPhy::DataErrorModelEnabled", BooleanValue (false));
	Config::SetDefault ("ns3::LteUePhy::NoiseFigure", DoubleValue(5.0));
	Config::SetDefault ("ns3::LteEnbPhy::NoiseFigure", DoubleValue(5.0));

	lteHelper->SetSchedulerType ("ns3::FdMtFfMacScheduler");

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
	p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));
	p2ph.SetDeviceAttribute ("Mtu", UintegerValue (1500));
	p2ph.SetChannelAttribute ("Delay", TimeValue (Seconds (0.010)));

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
	ueNodes.Create(1);

	std::ofstream myfile;
	myfile.open ("UEsPosition.txt");

	Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
	positionAlloc->Add(Vector(distance, 100, 0));
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
		// Set the default gateway for the UE
		Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (ueNode->GetObject<Ipv4> ());
		ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);
	}


	lteHelper->Attach (ueLteDevs.Get(0), enbLteDevs.Get(0));



	uint16_t sinkPort = 60000;

	PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort));

	ApplicationContainer sinkApps = packetSinkHelper.Install (ueNodes);
	sinkApps.Start (Seconds (1.0));
	sinkApps.Stop (Seconds (simTime));


	BulkSendHelper source ("ns3::TcpSocketFactory", InetSocketAddress (ueIpIface.GetAddress (0), sinkPort));
	source.SetAttribute ("MaxBytes", UintegerValue (30000));

	ApplicationContainer sourceApps = source.Install (remoteHost);
	sourceApps.Start (Seconds (2.0));
	sourceApps.Stop (Seconds (simTime));

	BulkSendHelper source2 ("ns3::TcpSocketFactory", InetSocketAddress (ueIpIface.GetAddress (0), sinkPort));
	source2.SetAttribute ("MaxBytes", UintegerValue (30000));

	ApplicationContainer sourceApps2 = source2.Install (remoteHost);
	sourceApps2.Start (Seconds (4.0));
	sourceApps2.Stop (Seconds (simTime));




	lteHelper->EnableTraces ();
	p2ph.EnablePcapAll("pcapTrace");
	internet.EnablePcapIpv4 ("mattia-tcp", remoteHost);

	Simulator::Stop(Seconds(simTime));
	Simulator::Run();
	Simulator::Destroy();
	return 0;

}


