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
	srand(1);
	std::remove("RNTI_IMSI_MAP.txt");
	std::remove("TDSchedulerLOG.txt");
	std::remove("FDSchedulerLOG.txt");



	int RBs=25;
	int earfcn1=500;
	int numberOfNodes = 10;
	bool fading=true;

//	double min_SNR_dB=5.0;
//	double mean_SNR_dB=15.0;
//	double mean_trace_gain=1;

	double simTime = 1.0;

	Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();

	lteHelper->SetSchedulerType ("ns3::FdMtFfMacScheduler");

	Config::SetDefault("ns3::LteAmc::AmcModel",EnumValue(LteAmc::PiroEW2010));			//Modello per il calcolo del CQI che valuta ciascun RB
	Config::SetDefault("ns3::LteEnbRrc::SrsPeriodicity",UintegerValue(160));			//Supporto per numerosi UEs
	Config::SetDefault("ns3::LteEnbPhy::TxPower",DoubleValue(30.0));					//Potenza trasmissiva (dBm) utilizzabile dagli eNode
	Config::SetDefault("ns3::LteUePhy::TxPower",DoubleValue(23.0));						//Potenza trasmissiva (dBm) utilizzabile dagli UEs
	Config::SetDefault ("ns3::LteSpectrumPhy::CtrlErrorModelEnabled", BooleanValue (false));
	Config::SetDefault ("ns3::LteSpectrumPhy::DataErrorModelEnabled", BooleanValue (false));
	Config::SetDefault ("ns3::LteUePhy::NoiseFigure", DoubleValue(5.0));
	Config::SetDefault ("ns3::LteEnbPhy::NoiseFigure", DoubleValue(5.0));
	lteHelper->SetSpectrumChannelType ("ns3::MultiModelSpectrumChannel");
	lteHelper->SetEnbDeviceAttribute("DlBandwidth",UintegerValue(RBs));
	lteHelper->SetEnbDeviceAttribute("DlEarfcn",UintegerValue(earfcn1));
	lteHelper->SetEnbDeviceAttribute("UlEarfcn",UintegerValue(earfcn1+18000));
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

	NodeContainer ueNodes;
	NodeContainer enbNodes;
	enbNodes.Create(1);
	ueNodes.Create(numberOfNodes);

	double SINRs[]={2.40,2.88,3.43,4.0455,4.77,5.676,6.72,8.168,10.339,14.68};


	Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();

	for (int i=0;i<numberOfNodes;i++)
	{
		double SNR_i_dB=SINRs[i];

		double K=33.331;	//This is the constant I need to compute the SNR for user

		double d=1000*pow(10,((K-SNR_i_dB)/20));

		std::cout<<"User "<<i+1<<", SNR (dB)"<<SNR_i_dB<<"dB), d="<<d<<" m"<<std::endl;

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

	// Attach a UE to a eNB
	lteHelper->Attach (ueLteDevs, enbLteDevs.Get (0));

	// Activate a data radio bearer
	enum EpsBearer::Qci q = EpsBearer::GBR_CONV_VOICE;
	EpsBearer bearer (q);
	lteHelper->ActivateDataRadioBearer (ueLteDevs, bearer);
	lteHelper->EnableTraces ();
	
	//Ptr<RadioBearerStatsCalculator> rlcStats = lteHelper->GetRlcStats ();
	//rlcStats->SetAttribute ("EpochDuration", TimeValue (Seconds (0.25)));
	//Ptr<RadioBearerStatsCalculator> pdcpStats = lteHelper->GetPdcpStats ();
	//pdcpStats->SetAttribute ("EpochDuration", TimeValue (Seconds (0.01)));
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


	std::cout<<"============ ACCESS PROBABILITIES (2) ========"<<std::endl;

	std::ifstream macFile ("DlMacStats.txt");
	std::vector<double> macLine(10);
	std::vector<int> winnedSlots2(numberOfNodes);

	if (macFile.is_open())
	{
		std::string line;
		while ( std::getline (macFile,line) )
		{
			int i=0;
			std::istringstream iss(line);
			while (iss>>macLine[i++]){}	//Read and parse an entire line
			int winnerIMSI=macLine[2];
			if (winnerIMSI>0)
				winnedSlots2[winnerIMSI-1]++;
		}
		int sum=0;
		for (int i=0;i<numberOfNodes;i++)
			sum+=winnedSlots2[i];

		for (int i=0;i<numberOfNodes;i++)
			std::cout<<"p("<<i+1<<")="<<double(winnedSlots2[i])/double(sum)<<std::endl;
	}
	macFile.close();

	std::cout<<"============ AVERAGE THROUGHPUT ============="<<std::endl;

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


