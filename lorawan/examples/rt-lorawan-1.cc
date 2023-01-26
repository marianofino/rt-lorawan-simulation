/*
 * This example is based on the network-server-example provided by the NS3
 * LoRaWAN module.
 */
 
/*

./waf --run "rt-lorawan --endDevicesCSV=/home/mariano/Workspace/Research/RT-LoRaWAN/simulator/ns3-lorawan-simulation/na-lorawan-simulator/scenarios/scenario_0_devices.csv --gatewaysCSV=/home/mariano/Workspace/Research/RT-LoRaWAN/simulator/ns3-lorawan-simulation/na-lorawan-simulator/scenarios/scenario_0_gateways.csv --msgSize=10"

*/

#include "ns3/point-to-point-module.h"
#include "ns3/network-server-helper.h"
#include "ns3/lora-channel.h"
#include "ns3/mobility-helper.h"
#include "ns3/lora-phy-helper.h"
#include "ns3/lorawan-mac-helper.h"
#include "ns3/lora-helper.h"
#include "ns3/gateway-lora-phy.h"
#include "ns3/periodic-sender.h"
#include "ns3/periodic-sender-helper.h"
#include "ns3/one-shot-sender-helper.h"
#include "ns3/log.h"
#include "ns3/string.h"
#include "ns3/command-line.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lora-device-address-generator.h"
#include "ns3/one-shot-sender-helper.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>
#include <string>

#define LENGTH 10000

using namespace ns3;
using namespace lorawan;
using namespace std;

string endDevicesCSVPath = "end_device_data.csv";
string gatewaysCSVPath = "gateway_data.csv";
double simulationHours = 999;
double msgPeriod = 20;
int msgSize = 10;
int naMode = 1;
int32_t budgetPerNode = 19;
int maxPackets = 1;

int totalPackets = 0;
int duplicatedRxByNS = 0;
int nonDuplicatedRxByNS = 0;
int discardedPacketsRxByNS = 0;
int discardedDupPacketsNotRxByNS = 0;
int discardedPacketsNotRxByNS = 0;
int64_t receivedPacketsUID[LENGTH]; // TODO change this structure??
int64_t discardedPacketsUID[LENGTH]; // TODO change this structure??
int64_t discardedNotRxPacketsUID[LENGTH]; // TODO change this structure??
int rxArrayLength = 0;
int discArrayLength = 0;
int discNotRxArrayLength = 0;
int priorityLvl = 1;

int nodeId = 0;

NodeContainer endDevices;
int totalEndDevices = 0;
int64_t lastPacketId = 99;

NS_LOG_COMPONENT_DEFINE ("RT_LoraWan");

void OnGWReceivedPackets (Ptr<Packet const> packet, uint32_t gatewayId)
{
  int64_t packetUid = (int64_t)packet->GetUid();

  // print the content of my packet on the standard output.
  //packet->Print (std::cout);
    
  if (lastPacketId != packetUid) {

    lastPacketId = packetUid;
    nodeId++;

    if (nodeId < totalEndDevices) {
      // TODO set message size
      OneShotSenderHelper appHelper = OneShotSenderHelper();
      appHelper.SetSendTime (Seconds (30));
      ApplicationContainer appContainer = appHelper.Install (endDevices.Get(nodeId));
    }
    
  }
  
  uint8_t dataRate = endDevices.Get(nodeId - 1)->GetDevice (0)->GetObject<LoraNetDevice> ()->GetMac ()->GetObject<ClassAEndDeviceLorawanMac> ()->GetDataRate ();
  
  uint8_t sf = endDevices.Get(nodeId - 1)->GetDevice (0)->GetObject<LoraNetDevice> ()->GetMac ()->GetSfFromDataRate (dataRate);
  
  std::cout << (nodeId) << "," << (gatewayId-totalEndDevices+1) << "," << (int) sf << std::endl;

  int64_t *uidPosition = std::find(std::begin(receivedPacketsUID), std::end(receivedPacketsUID), packetUid);
  if (uidPosition != std::end(receivedPacketsUID)) {
    duplicatedRxByNS++;
  } else {
    nonDuplicatedRxByNS++;
    receivedPacketsUID[rxArrayLength++] = packetUid;
  }

}

int main (int argc, char *argv[])
{

  // Enable the packet printing through Packet::Print command.
  Packet::EnablePrinting ();

  bool verbose = false;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Whether to print output or not", verbose);
  cmd.AddValue ("endDevicesCSV", "Path to file containing end devices location to include in the simulation", endDevicesCSVPath);
  cmd.AddValue ("gatewaysCSV", "Path to file containing gateways location to include in the simulation", gatewaysCSVPath);
  cmd.AddValue ("simulationHours", "The time in hours for which to simulate", simulationHours);
  cmd.AddValue ("msgSize", "The size of the packet payload of the transmitted messages", msgSize);
  cmd.Parse (argc, argv);

  for (int i=0; i < LENGTH; i++) {
    receivedPacketsUID[i] = -1;
    discardedPacketsUID[i] = -1;
  }

  // Logging
  //////////

  //LogComponentEnable ("Forwarder", LOG_LEVEL_ALL);
  //LogComponentEnable ("GatewayLorawanMac", LOG_LEVEL_ALL);
  //LogComponentEnable ("LorawanMacHelper", LOG_LEVEL_ALL);
  //LogComponentEnable ("EndDeviceLorawanMac", LOG_LEVEL_ALL);
  //LogComponentEnable ("NA_LorawanMacHelper", LOG_LEVEL_ALL);
  LogComponentEnableAll (LOG_PREFIX_FUNC);
  LogComponentEnableAll (LOG_PREFIX_NODE);
  LogComponentEnableAll (LOG_PREFIX_TIME);

  // Create a simple wireless channel
  ///////////////////////////////////

  Ptr<LogDistancePropagationLossModel> loss = CreateObject<LogDistancePropagationLossModel> ();
  loss->SetPathLossExponent (3.76);
  loss->SetReference (1, 7.7);

  Ptr<PropagationDelayModel> delay = CreateObject<ConstantSpeedPropagationDelayModel> ();

  Ptr<LoraChannel> channel = CreateObject<LoraChannel> (loss, delay);

  // Helpers
  //////////

  // End Device mobility
  MobilityHelper mobilityEd, mobilityGw;
  Ptr<ListPositionAllocator> positionAllocEd = CreateObject<ListPositionAllocator> ();

/*
  vector<int> delays;
  vector<int> periods;
*/

  ifstream endDevicesCSV(endDevicesCSVPath);

  string line, word;
  vector<string> row;

  if (endDevicesCSV.is_open())
  {
    getline (endDevicesCSV, line); // ignore headers
    while ( getline (endDevicesCSV, line) )
    {
      row.clear();
      stringstream s(line);
      while (getline(s, word, ','))
      {
        row.push_back(word);
      }
      positionAllocEd->Add (Vector (stof(row[1]), stof(row[2]), stof(row[3])));
/*
      delays.push_back(stoi(row[5]) * (stoi(row[6]) - 1));
      periods.push_back(stoi(row[4]) * (stoi(row[5])));
*/
      totalEndDevices++;
    }
    endDevicesCSV.close();
  }

  mobilityEd.SetPositionAllocator (positionAllocEd);
  mobilityEd.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

  // Gateway mobility
  Ptr<ListPositionAllocator> positionAllocGw = CreateObject<ListPositionAllocator> ();

  ifstream gatewaysCSV(gatewaysCSVPath);
  int totalGateways = 0;

  if (gatewaysCSV.is_open())
  {
    getline (gatewaysCSV, line); // ignore headers
    while ( getline (gatewaysCSV, line) )
    {
      row.clear();
      stringstream s(line);
      while (getline(s, word, ','))
      {
        row.push_back(word);
      }
      positionAllocGw->Add (Vector (stof(row[1]), stof(row[2]), stof(row[3])));
      totalGateways++;
    }
    gatewaysCSV.close();
  }

  mobilityGw.SetPositionAllocator (positionAllocGw);
  mobilityGw.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

  // Create the LoraPhyHelper
  LoraPhyHelper phyHelper = LoraPhyHelper ();
  phyHelper.SetChannel (channel);

  // Create the LorawanMacHelper
  LorawanMacHelper macHelper = LorawanMacHelper ();

  // Create the LoraHelper
  LoraHelper helper = LoraHelper ();

  // Create EDs
  /////////////

  endDevices.Create (totalEndDevices);
  mobilityEd.Install (endDevices);

  // Create a LoraDeviceAddressGenerator
  uint8_t nwkId = 0;
  uint32_t nwkAddr = 1864;
  Ptr<LoraDeviceAddressGenerator> addrGen = CreateObject<LoraDeviceAddressGenerator> (nwkId,nwkAddr);

  // Create the LoraNetDevices of the end devices
  phyHelper.SetDeviceType (LoraPhyHelper::ED);
  macHelper.SetDeviceType (LorawanMacHelper::ED_A);
  macHelper.SetAddressGenerator (addrGen);
  macHelper.SetRegion (LorawanMacHelper::EU);
  helper.Install (phyHelper, macHelper, endDevices);
  
  OneShotSenderHelper appHelper = OneShotSenderHelper();
  appHelper.SetSendTime (Seconds (30));
  ApplicationContainer appContainer = appHelper.Install (endDevices.Get(nodeId));

  ////////////////
  // Create GWs //
  ////////////////

  NodeContainer gateways;
  gateways.Create (totalGateways);
  mobilityGw.Install (gateways);

  // Create the LoraNetDevices of the gateways
  phyHelper.SetDeviceType (LoraPhyHelper::GW);
  macHelper.SetDeviceType (LorawanMacHelper::GW);
  helper.Install (phyHelper, macHelper, gateways);

  ////////////
  // Create NS
  ////////////

  NodeContainer networkServers;
  networkServers.Create (1);

  // Install the NetworkServer application on the network server
  NetworkServerHelper networkServerHelper;
  networkServerHelper.SetGateways (gateways);
  networkServerHelper.SetEndDevices (endDevices);
  networkServerHelper.Install (networkServers);
  
  for (NodeContainer::Iterator j = gateways.Begin (); j != gateways.End (); ++j)
  {
    Ptr<Node> node = *j;
    Ptr<GatewayLorawanMac> gw = node->GetDevice (0)->GetObject<LoraNetDevice> ()->GetMac ()->GetObject<GatewayLorawanMac> ();
    gw->TraceConnectWithoutContext("GWReceivedPacket", MakeCallback(&OnGWReceivedPackets));
  }
  
  // Set Spreading Factor
  for (NodeContainer::Iterator j = endDevices.Begin (); j != endDevices.End (); ++j)
  {
    Ptr<Node> node = *j;
    node->GetDevice (0)->GetObject<LoraNetDevice> ()->GetMac ()->GetObject<ClassAEndDeviceLorawanMac> ()->SetDataRate (0);
  }
  
  // Set spreading factors up
  //macHelper.SetSpreadingFactorsUp (endDevices, gateways, channel);
  
  // Print CSV header
  std::cout << "nodeId" << "," << "gatewayId" << "," << "sf" << std::endl;
  
  // Start simulation
  Simulator::Stop(Seconds (60*60*simulationHours));
  Simulator::Run();
  Simulator::Destroy ();

  return 0;
}
