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

	std::remove("RNTI_IMSI_MAP.txt");


	double min_SNR_dB=10.0;
	double mean_SNR_dB=15.0;

	int numberOfNodes=2;


	double simTime = 60;
	bool fading=true;

	Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
	lteHelper->SetSpectrumChannelType ("ns3::MultiModelSpectrumChannel");

	Config::SetDefault ("ns3::TcpSocket::SegmentSize",UintegerValue(1024));
	Config::SetDefault ("ns3::LteEnbRrc::EpsBearerToRlcMapping", EnumValue (ns3::LteEnbRrc::RLC_UM_ALWAYS));
	Config::SetDefault("ns3::LteAmc::AmcModel",EnumValue(LteAmc::PiroEW2010));
	Config::SetDefault("ns3::LteEnbRrc::SrsPeriodicity",UintegerValue(160));
	Config::SetDefault("ns3::LteEnbPhy::TxPower",DoubleValue(30.0));
	Config::SetDefault("ns3::LteUePhy::TxPower",DoubleValue(23.0));
	Config::SetDefault ("ns3::LteSpectrumPhy::CtrlErrorModelEnabled", BooleanValue (false));
	Config::SetDefault ("ns3::LteSpectrumPhy::DataErrorModelEnabled", BooleanValue (false));
	Config::SetDefault ("ns3::LteUePhy::NoiseFigure", DoubleValue(5.0));
	Config::SetDefault ("ns3::LteEnbPhy::NoiseFigure", DoubleValue(5.0));

	lteHelper->SetAttribute ("PathlossModel", StringValue ("ns3::FriisPropagationLossModel"));

	if (fading)
	{
		lteHelper->SetFadingModel("ns3::TraceFadingLossModel");
		lteHelper->SetFadingModelAttribute ("TraceFilename", StringValue ("src/lte/model/fading-traces/R_flat_60.fad"));
		lteHelper->SetFadingModelAttribute ("TraceLength", TimeValue (Seconds (60.0)));
		lteHelper->SetFadingModelAttribute ("SamplesNum", UintegerValue (60000));
		lteHelper->SetFadingModelAttribute ("WindowSize", TimeValue (Seconds (0.5)));
		lteHelper->SetFadingModelAttribute ("RbNum", UintegerValue (100));
	}

	lteHelper->SetSchedulerType ("ns3::TdMtFfMacScheduler");

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


	/*Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
	for(int i=0;i<numberOfNodes;i++)
	{positionAlloc->Add(Vector((i+1)*distance, 0, 0));}

	MobilityHelper mobility;
	mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
	mobility.SetPositionAllocator(positionAlloc);
	mobility.Install(ueNodes);*/

	double min_SNR=pow(10,(min_SNR_dB/10));
	double mean_SNR=pow(10,(mean_SNR_dB/10));

	double range=(mean_SNR-min_SNR)*2;

	double delta=range/(numberOfNodes-1);

	Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();

	for (int i=0;i<numberOfNodes;i++)
	{
		double SNR_i=min_SNR+i*delta;

		double SNR_i_dB=10*log10(SNR_i);

		double K=33.331;	//This is the constant I need to compute the SNR for user

		double d=1000*pow(10,((K-SNR_i_dB)/20));

		std::cout<<"User "<<i+1<<", SNR="<<SNR_i<<" ("<<SNR_i_dB<<"dB), d="<<d<<" m"<<std::endl;

		positionAlloc->Add (Vector(d, 0, 0));
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

	// Assign IP address to UEs
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



	uint16_t sinkPort = 60000;

	for (uint32_t u = 0; u < ueNodes.GetN (); ++u)
	{
		Ptr<Node> ue = ueNodes.Get (u);
		++sinkPort;

		//Configure the client to receive data from the server and to ACK them
		PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
		ApplicationContainer sinkApps = packetSinkHelper.Install (ue);
		sinkApps.Start (Seconds (0.5));
		sinkApps.Stop (Seconds (simTime));


		//Setup the server on the remote host
		BulkSendHelper source ("ns3::TcpSocketFactory", InetSocketAddress (ueIpIface.GetAddress (u), sinkPort));
		source.SetAttribute ("MaxBytes", UintegerValue (0));
		ApplicationContainer sourceApps = source.Install (remoteHost);
		sourceApps.Start (Seconds (0.7+0.1*double(u)));
		sourceApps.Stop (Seconds (simTime));

	}

	lteHelper->EnableTraces ();
	p2ph.EnablePcapAll("pcapTrace");
	internet.EnablePcapIpv4 ("remoteHost-tcp", remoteHost);

	Simulator::Stop(Seconds(simTime));
	Simulator::Run();
	Simulator::Destroy();


	std::cout<<"=========== Average SINR per user ==========="<<std::endl;

	std::ifstream sinrFile ("DlRsrpSinrStats.txt");
	std::vector<double> sinrLine(6);
	std::vector<double> meanSinr(numberOfNodes);
	std::vector<double> sinrCounter(numberOfNodes);
	if (sinrFile.is_open())
	{
		std::string line;
		while ( std::getline (sinrFile,line) )
		{
			int i=0;
			std::istringstream iss(line);
			while (iss>>sinrLine[i++]){}	//Read and parse an entire line
			int IMSI=sinrLine[2];
			if (IMSI>0)
			{
				meanSinr[IMSI-1]+=sinrLine[5];
				sinrCounter[IMSI-1]++;
			}
		}
		for (int i=0;i<numberOfNodes;i++)
			std::cout<<"User "<<i+1<<", mean SINR="<<(meanSinr[i]/sinrCounter[i])<<" ("<<10*log10(meanSinr[i]/sinrCounter[i])<<" dB)"<<std::endl;

	}
	sinrFile.close();


	std::cout<<"=========== Average throughput per user ==========="<<std::endl;




	std::ifstream txData ("DlRlcStats.txt");
	std::vector<double> loggedData(18);
	std::vector<double> txBytes(numberOfNodes);
	int overallTxBytes=0;

	if (txData.is_open())
	{
		std::string line;
		while ( std::getline (txData,line) )
		{
			int i=0;
			std::istringstream iss(line);
			while (iss>>loggedData[i++]){}
			int IMSI=loggedData[3];
			txBytes[IMSI-1]+=loggedData[7];
			overallTxBytes+=loggedData[7];
		}
		txData.close();
	}

	for (int i=0;i<numberOfNodes;i++)
		std::cout<<"User "<<i+1<<" has sent "<<txBytes[i]<<" bytes in "<<simTime<<" seconds. R("<<i+1<<")="<<(((txBytes[i]*8)/simTime)/1000000)<<" Mbit/s"<<std::endl;

	std::cout<<"============ AVERAGE CELL THROUGHPUT ============="<<std::endl;

	std::cout<<"Cell throughput: "<<overallTxBytes*8/simTime/1000000<<" Mbit/s"<<std::endl;

	std::cout<<"============ FAIRNESS INDEX ============="<<std::endl;
	std::cout<<"Jain's fairness index: J=";
	double numeratore=0;
	for (int i=0;i<numberOfNodes;i++)
		numeratore+=(txBytes[i]*8)/simTime;

	numeratore=pow(numeratore,2);

	double denominatore=0;
	for (int i=0;i<numberOfNodes;i++)
		denominatore+=pow((txBytes[i]*8)/simTime,2);

	double J=numeratore/(numberOfNodes*denominatore);
	std::cout<<J<<" (system is "<<(J-1/double(numberOfNodes))/(1-1/double(numberOfNodes)) *100<<"% fair.)"<<std::endl;
	return 0;
}


