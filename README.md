# Empirical Real-Time Guarantees to LoRaWAN

This repository has files containing code that has been used for simulating an algorithm to add Real-Time capabilities to LoRaWAN. The directory is organized as follows:

* **rt-lorawan-simulator**: contains the script to create scenarios and run simulations (a wrapper of NS3).
* **lorawan**: contains the modified NS3 LoRaWAN module required for the simulations.
* **scenarios and results**: contains sample scenarios created to test the algorithm and its corresponding results.

## Prerequisites

* Node.js (tested on version 16.17.0)
* ns-3 v3.29 prerequisites: https://www.nsnam.org/docs/release/3.29/tutorial/html/getting-started.html#prerequisites

## Instalation instructions

1. After installing and configuring the LoRaWAN module, open a terminal and navigate to the NS3 directory

2. Execute:

```
$ ./waf configure --enable-examples
$ ./waf build
```

3. Then navigate to the `./rt-lorawan-simulator` directory

4. Execute:

```
$ npm install
```

## Running the scripts

### Creation of scenarios

1. Navigate to the `./rt-lorawan-simulator` directory

2. Configure the file `config.yaml` that sets the characteristics of the scenarios to be created

3. From a terminal, execute:

```
$ node index.js scenarios
```

4. Scenarios are stored in the directory `scenarios`. For each one of them there is a CSV with the gateway information, a CSV with the devices information, and an SVG with a drawing of the location of gateways and devices. Also, there is an empty `schedules` directory, where the schedules for those scenarios can be loaded (the schedules are built with another tool).

### Simulate RT Schedules

The schedules must be loaded to the `schedules` directory with the name `scenario_<X>_schedule_sf<Y>.csv`, where <X> is the scenario number (same as the one generated by this tool) and <Y> is the Spreading Factor for the corresponding schedule.

1. Navigate to the `./rt-lorawan-simulator` directory

2. From a terminal, execute:

```
$ node index.js run
```

### Simulate Random LoRaWAN Schedules

To generate a random LoRaWAN schedule for an existing scenario:

1. Navigate to the `./rt-lorawan-simulator` directory

2. From a terminal, execute:

```
$ node randomSchedule.js <scenarioId>
```

3. To run the simulation, execute:

```
$ node index.js run --mode random
```

4. The results are stored in the `schedules` directory, with a similar name to the schedules, except that they have a `_results` suffix.

## Simulation results interpretation

The simulation results file is a CSV containing the following fields:	

* deviceId: Id of the End Device, taken from the input file.
* packetUid: Unique packet id across the whole network of packets send by End Devices, assigned by NS3.
* task: As in RT systems, a task represent an instance of a job; in this case the job is sending the packet. It could also be seen as the instance number of the current window.
* slot: The slot number of the window assigned to this End Device by the scheduling algorithm.
* theoreticalStartTime: The time at which the assigned slot should start in theory.
* deviceTxStartTime: The time at which the End Device has started the Tx process in the simulation. Ideally, it should match the slotTheoreticalStartTime.
* deviceTxFinishTime: The time at which the End Device has finished Tx.
* deadline: The maximum slot to transmit the packet, relative to the start of the current window, before it expires.
* deadlineFinishTime: The time at which the slot of the deadline finishes.
* delayInStartTime: Result of: deviceTxStartTime - slotTheoreticalStartTime.
* delayInDeadline: Result of: deviceTxFinishTime - deadlineFinishTime. A negative number means that the device finish transmitting after its deadline, and so its consider to have not comply the deadline.
* onTime: Its 1 if delayInDeadline >= 0, and 0 if delayInDeadline < 0.
* gatewaysReceived: The gateways ids that actually received the packet in the NS3 simualtion, separed by the ":" symbol.
* gatewaysExpected: The gateways ids that were expected to receive the packet according to the input.
* gatewaysLost: The gateways ids that didn't receive the packet and were supposed to.
* gatewaysReceivedQty: The number of gateways in gatewaysReceived.
* gatewaysExpectedQty: The number of gateways in gatewaysReceivedQty.
* gatewaysLostQty: The number of gateways in gatewaysLost.
* otherLostLoRaPackets: Deprecated.
* receivedByNS: Whether the NS has received at least one copy of this packet.
* inteferingPackets: Packets ids that have provoked a collision.
* inteferingNodes: Nodes that have Tx packets that have provoked a collision.
* collisionType: If no collision happened, it is 0. If a collision happened because there was interference from another packet meant for another gateway, value is 1. If a collision happened because two packets tried to Tx to the same gateway at the same time its value is 2.
