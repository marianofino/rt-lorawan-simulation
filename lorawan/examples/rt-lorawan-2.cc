/*
 * This example is based on the network-server-example provided by the NS3
 * LoRaWAN module.
 */
 
/*

./waf --run "rt-lorawan-2 --endDevicesCSV=/home/mariano/Workspace/Research/RT-LoRaWAN/simulator/rt-lorawan-simulator/scenarios/scenario_1_devices.csv --gatewaysCSV=/home/mariano/Workspace/Research/RT-LoRaWAN/simulator/rt-lorawan-simulator/scenarios/scenario_1_gateways.csv --msgSize=10"

*/

#include "ns3/point-to-point-module.h"
#include "ns3/forwarder-helper.h"
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
double simulationHours = 1;
int msgSize = 100;

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

vector<int> devicesId;
vector<int> devicesAddress;

vector<int> deadlines;
vector<int> slotNumbers;
vector<int> windowLengths;
vector<double> slotLengths;
vector<string> gatewayList;

bool first = true;

int tasksCounter[10000] = { 0 };

NS_LOG_COMPONENT_DEFINE ("RT_LoraWan");

int getIndex(vector<int> v, int K)
{
    int index = -1;
    auto it = find(v.begin(), v.end(), K);
  
    // If element was found
    if (it != v.end()) 
    {
      
        // calculating the index
        // of K
        index = it - v.begin();
    }
    
    return index;
}

void OnEDStartTxPackets (Ptr<Packet const> packet) {
  int64_t packetUid = (int64_t)packet->GetUid();
  
  Ptr<Packet> packetCopy = packet->Copy ();
  LorawanMacHeader receivedMacHdr;
  packetCopy->RemoveHeader (receivedMacHdr);
  LoraFrameHeader receivedFrameHdr;
  packetCopy->RemoveHeader (receivedFrameHdr);

  uint32_t address = receivedFrameHdr.GetAddress().Get();
  
  int i = getIndex(devicesAddress, address);
  
  tasksCounter[i]++;
  
  double theoreticalStartTime = ( windowLengths[i] * tasksCounter[i] - (windowLengths[i] - slotNumbers[i]) - 1 ) * slotLengths[i];
  
  double txStartTime = Simulator::Now().GetSeconds ();
  double delay = txStartTime - theoreticalStartTime;
  
  //std::cout << "get size " << packet->GetSize() << std::endl;
  
  if (first)
    first = false;
  else
    std::cout << "," << std::endl;
  
  std::cout << "{ " <<
                  "\"timestamp\": " << Simulator::Now().GetSeconds () << ", " <<
                  "\"eventType\": 1, " <<
                  "\"deviceId\": " << devicesId[i] << ", " <<
                  "\"packetUid\": " << packetUid << ", " <<
                  "\"task\": " << tasksCounter[i] << ", " <<
                  "\"slot\": " << slotNumbers[i] << ", " <<
                  "\"slotTheoreticalStartTime\": " << theoreticalStartTime << ", " <<
                  "\"deviceTxStartTime\": " << txStartTime << ", " <<
                  "\"gateways\": \"" << gatewayList[i] << "\", " <<
                  "\"delayInStartTime\": " << delay << " }" << std::endl;

  //std::cout << Simulator::Now().GetSeconds () << ",1," << devicesId[i] << "," << packetUid << "," << tasksCounter[i] << "," << slotNumbers[i] << "," << theoreticalStartTime << "," << txStartTime << "," << delay << std::endl;
}



void OnEDFinishTxPackets (Ptr<Packet const> packet) {
  int64_t packetUid = (int64_t)packet->GetUid();
  
  Ptr<Packet> packetCopy = packet->Copy ();
  LorawanMacHeader receivedMacHdr;
  packetCopy->RemoveHeader (receivedMacHdr);
  LoraFrameHeader receivedFrameHdr;
  packetCopy->RemoveHeader (receivedFrameHdr);

  uint32_t address = receivedFrameHdr.GetAddress().Get();
  
  int i = getIndex(devicesAddress, address);
  
  double txFinishTime = Simulator::Now().GetSeconds ();

  double deadlineFinishTime = ( windowLengths[i] * tasksCounter[i] - (windowLengths[i] - slotNumbers[i])) * slotLengths[i];
  
  double delayInDeadline = deadlineFinishTime - txFinishTime;
  
  int onTime = delayInDeadline >= 0 ? 1 : 0;
  
  
  //std::cout << "get size " << packet->GetSize() << std::endl;
  
  if (first)
    first = false;
  else
    std::cout << "," << std::endl;
  
  std::cout << "{ " <<
                  "\"timestamp\": " << Simulator::Now().GetSeconds () << ", " <<
                  "\"eventType\": 2, " <<
                  "\"deviceId\": " << devicesId[i] << ", " <<
                  "\"packetUid\": " << packetUid << ", " <<
                  "\"deviceTxFinishTime\": " << txFinishTime << ", " <<
                  "\"deadline\": " << deadlines[i] << ", " <<
                  "\"deadlineFinishTime\": " << deadlineFinishTime << ", " <<
                  "\"delayInDeadline\": " << delayInDeadline << ", " <<
                  "\"onTime\": " << onTime << " }" << std::endl;

  //std::cout << Simulator::Now().GetSeconds () << ",1," << devicesId[i] << "," << packetUid << "," << tasksCounter[i] << "," << slotNumbers[i] << "," << theoreticalStartTime << "," << txStartTime << "," << delay << std::endl;
}

void OnNSReceivedPackets (Ptr<Packet const> packet)
{
  int64_t packetUid = (int64_t)packet->GetUid();
  
  Ptr<Packet> packetCopy = packet->Copy ();
  LorawanMacHeader receivedMacHdr;
  packetCopy->RemoveHeader (receivedMacHdr);
  LoraFrameHeader receivedFrameHdr;
  packetCopy->RemoveHeader (receivedFrameHdr);

  uint32_t address = receivedFrameHdr.GetAddress().Get();
  
  int i = getIndex(devicesAddress, address);
  
  if (first)
    first = false;
  else
    std::cout << "," << std::endl;
  
  std::cout << "{ " <<
                  "\"timestamp\": " << Simulator::Now().GetSeconds () << ", " <<
                  "\"eventType\": 4, " <<
                  "\"deviceId\": " << devicesId[i] << ", " <<
                  "\"packetUid\": " << packetUid << "}" << std::endl;
  
  //std::cout << Simulator::Now().GetSeconds () << ",2," << devicesId[i] << "," << packetUid << std::endl;

}

void OnGWReceivedPackets (Ptr<Packet const> packet, uint32_t gatewayId)
{
  int64_t packetUid = (int64_t)packet->GetUid();
  
  Ptr<Packet> packetCopy = packet->Copy ();
  LorawanMacHeader receivedMacHdr;
  packetCopy->RemoveHeader (receivedMacHdr);
  LoraFrameHeader receivedFrameHdr;
  packetCopy->RemoveHeader (receivedFrameHdr);

  uint32_t address = receivedFrameHdr.GetAddress().Get();
  
  int i = getIndex(devicesAddress, address);
  
  if (first)
    first = false;
  else
    std::cout << "," << std::endl;
  
  std::cout << "{ " <<
                  "\"timestamp\": " << Simulator::Now().GetSeconds () << ", " <<
                  "\"eventType\": 3, " <<
                  "\"deviceId\": " << devicesId[i] << ", " <<
                  "\"packetUid\": " << packetUid << ", " <<
                  "\"gatewayId\": " << (gatewayId-totalEndDevices+1) << " }" << std::endl;
  
  //std::cout << Simulator::Now().GetSeconds () << ",3," << devicesId[i] << "," << packetUid << std::endl;
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
//  LogComponentEnable ("EndDeviceLorawanMac", LOG_LEVEL_ALL);
  //LogComponentEnable ("NA_LorawanMacHelper", LOG_LEVEL_ALL);
  //LogComponentEnable ("NetworkServer", LOG_LEVEL_ALL);
  //LogComponentEnable ("PeriodicSenderHelper", LOG_LEVEL_ALL);
  
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

  vector<double> delays;
  vector<double> periods;
  vector<int> sfs;

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
      
      int deviceId = stoi(row[0]);
      
      vector<int>::iterator it = find(devicesId.begin(), devicesId.end(), deviceId);
      
      if (it == devicesId.end()) {
      
        positionAllocEd->Add (Vector (stof(row[1]), stof(row[2]), stof(row[3])));
        
        devicesId.push_back(deviceId);
        
        int slotNumber = stoi(row[9]);
        int windowLength = stoi(row[7]);
        double slotLength = stod(row[8]);
        int deadline = stoi(row[4]);
        
        slotNumbers.push_back(slotNumber);
        slotLengths.push_back(slotLength);
        windowLengths.push_back(windowLength);
        deadlines.push_back(deadline);
        
        sfs.push_back(stoi(row[5]));

        delays.push_back(slotLength * (slotNumber - 1)); // slotlength * slotnumber (-1 because slot number start with 1)
        periods.push_back(slotLength * windowLength); // slotlength * windowlength
        
        string gatewayId = row[6];
        
        gatewayList.push_back(gatewayId);

        totalEndDevices++;
      
      } else {
      
        string gatewayId = row[6];
        gatewayList[it - devicesId.begin()] = gatewayList[it - devicesId.begin()] + ":" + gatewayId;
      
      }
      
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
  
  PeriodicSenderHelper appHelper = PeriodicSenderHelper ();
  appHelper.SetPeriod (periods);
  //appHelper.SetPacketSize (59);
  appHelper.SetSFs (sfs);
  appHelper.SetInitialDelay (delays);
  ApplicationContainer appContainer = appHelper.Install (endDevices);
  
  // Get End Devices Addresses
  for (NodeContainer::Iterator j = endDevices.Begin (); j != endDevices.End (); ++j)
  {
    Ptr<Node> node = *j;
    uint32_t address = node->GetDevice (0)->GetObject<LoraNetDevice> ()->GetMac ()->GetObject<ClassAEndDeviceLorawanMac> ()->GetDeviceAddress().Get();
    
    node->GetDevice (0)->GetObject<LoraNetDevice> ()->GetMac ()->TraceConnectWithoutContext("SentNewPacket", MakeCallback(&OnEDStartTxPackets));
    node->GetDevice (0)->GetObject<LoraNetDevice> ()->GetMac ()->TraceConnectWithoutContext("FinishSendingPacket", MakeCallback(&OnEDFinishTxPackets));
    
    devicesAddress.push_back(address);
  }
  
  // Set Spreading Factor

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

  // Install the Forwarder application on the gateways
  ForwarderHelper forwarderHelper;
  forwarderHelper.Install (gateways);

  for (NodeContainer::Iterator j = networkServers.Begin (); j != networkServers.End (); ++j)
  {
    Ptr<Node> node = *j;
    Ptr<NetworkServer> nwServer = node->GetApplication(0)->GetObject<NetworkServer> ();
    nwServer->TraceConnectWithoutContext("ReceivedPacket", MakeCallback(&OnNSReceivedPackets));
  }

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
  //std::cout << "timestamp,eventType,deviceId,packetUid,sf,packetSize" << std::endl;
  
  // Start simulation
  std::cout << "[" << std::endl;
  Simulator::Stop(Seconds (60*60*simulationHours));
  Simulator::Run();
  Simulator::Destroy ();
  std::cout << "]" << std::endl;
  
  return 0;
}
