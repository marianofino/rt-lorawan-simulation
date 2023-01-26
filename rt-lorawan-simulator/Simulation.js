const { spawn } = require('child_process')
const { Readable } = require("stream")
const csv = require('csv-parser')

function Simulation(scenarioPath, defaultValues, ns3) {

  const msgSize = defaultValues.msgSize

  const endDevicesCSV = scenarioPath.devices
  const gatewaysCSV = scenarioPath.gateways

  let simArgs = {
    endDevicesCSV,
    gatewaysCSV
  }

  if (msgSize !== undefined) simArgs.msgSize = msgSize
  

  this.run = (mode = 2) => {

    let command = `cd ${ns3.wafDir} && ./waf --run "rt-lorawan-${mode}`

    Object.keys(simArgs).forEach((a) => {
      const v = simArgs[a]
      command += ` --${a}=${v}`
    })

    command += '"'
    
    console.log(command)
    
    return new Promise((res, rej) => {

      console.log(`Executing command:\n${command}`)

      const child = spawn(command, {
        shell: true
      })

      let storeOutput = false
      let output = ''

      child.stdout.on('data', data => {

        if (storeOutput) {
          output += data
        }

        if (data.toString().slice(0, 7) === '\'build\'')
          storeOutput = true

      })

      child.on('error', (error) => {
        console.error(output)
        rej(error)
      })

      child.on('close', (code) => {
        try {
        
          const data = JSON.parse(output)
          
          const parsedResults = []
          
          // filter all sent packets (event 1)
          const event1Data = data.filter((d) => d.eventType === 1)
          
          for (const startSending of event1Data) {
          
            // get finish sending packets (event 2)
            const finishSending = data.find((d) => d.eventType === 2 && d.packetUid === startSending.packetUid)
            const gatewayReceived = data.filter((d) => d.eventType === 3 && d.packetUid === startSending.packetUid)
            const nsReceived = data.find((d) => d.eventType === 4 && d.packetUid === startSending.packetUid)
            const gatewayInterference = data.filter((d) => d.eventType === 5 && d.packetUid === startSending.packetUid)
            
            if (finishSending === undefined)
              continue
            
            const gatewaysReceivedQty = gatewayReceived.length
            const gatewaysExpectedQty = startSending.gateways.split(':').length
            const gatewaysLostQty = gatewayInterference.length
            
            const newRow = {
              deviceId: startSending.deviceId,
              packetUid: startSending.packetUid,
              task: startSending.task,
              deadline: finishSending.deadline,
              theoreticalStartTime: startSending.slotTheoreticalStartTime,
              deviceTxStartTime: startSending.deviceTxStartTime,
              delayInStartTime: startSending.deviceTxStartTime - startSending.slotTheoreticalStartTime,
              deviceTxFinishTime: finishSending.deviceTxFinishTime,
              deadlineFinishTime: finishSending.deadlineFinishTime,
              delayInDeadline: finishSending.deadlineFinishTime - finishSending.deviceTxFinishTime,
              onTime: (finishSending.deadlineFinishTime - finishSending.deviceTxFinishTime) >= 0 ? 1 : 0,
              gatewaysReceived: gatewayReceived.map((d) => d.gatewayId).join(':'),
              gatewaysExpected: startSending.gateways,
              gatewaysReceivedQty: gatewaysReceivedQty,
              gatewaysExpectedQty: gatewaysExpectedQty,
              gatewaysLost: gatewayInterference.map((d) => d.gatewayId).join(':'),
              gatewaysLostQty: gatewaysLostQty,
              otherLostLoRaPackets: gatewaysExpectedQty - gatewaysReceivedQty - gatewaysLostQty,
              receivedByNS: nsReceived ? 1 : 0
            }
            
            if (startSending.slot)
              newRow.slot = startSending.slot
            
            if (startSending.delay)
              newRow.delay = startSending.delay
            
            if (gatewayInterference.length > 0) {
              newRow.inteferingPackets = gatewayInterference.map((d) => d.inteferingPackets).join(':')
              newRow.inteferingNodes = ''
              newRow.collisionType = 0
            } else {
              newRow.inteferingPackets = ''
              newRow.inteferingNodes = ''
              newRow.collisionType = 0
            }
          
            parsedResults.push(newRow)
          
          }
          
          for (const row of parsedResults) {
            if (row.inteferingPackets != '') {
              const inteferingPackets = row.inteferingPackets.split(':')
              
              const gatewaysOfNode = row.gatewaysExpected.split(':')
              
              // get nodes
              for (const p of inteferingPackets) {
                const packetData = data.find((d) => d.eventType === 1 && d.packetUid === parseInt(p))
                const gatewaysOfInterferingPacket = packetData.gateways.split(':')
                for (const g of gatewaysOfInterferingPacket) {
                  row.inteferingNodes += packetData.deviceId + ':'
                  if (gatewaysOfNode.indexOf(g) >= 0)
                    row.collisionType = 2
                }
                
                if (row.collisionType === 0)
                  row.collisionType = 1
              }
            }
                
            if (row.inteferingNodes[row.inteferingNodes.length - 1] === ':')
              row.inteferingNodes = row.inteferingNodes.slice(0, -1)
          }
          
          //console.log(parsedResults)
          
          //process.exit()
          
          res(parsedResults)
          
          /*
          Readable.from([output])
                  .pipe(csv())
                  .on('data', (data) => parsedResults.push(data))
                  .on('end', () => {
                    res(parsedResults)
                  })
                  
          */

        } catch (e) {
          console.error(output)
          rej(e)
        }
      })

    })
  
  }

  this.getSFandGWs = () => {

    let command = `cd ${ns3.wafDir} && ./waf --run "rt-lorawan-1`

    Object.keys(simArgs).forEach((a) => {
      const v = simArgs[a]
      command += ` --${a}=${v}`
    })

    command += '"'

    return new Promise((res, rej) => {

      console.log(`Executing command:\n${command}`)

      const child = spawn(command, {
        shell: true
      })

      let storeOutput = false
      let output = ''

      child.stdout.on('data', data => {

        if (storeOutput) {
          output += data
        }

        if (data.toString().slice(0, 7) === '\'build\'')
          storeOutput = true

      })

      child.on('error', (error) => {
        console.error(output)
        rej(error)
      })

      child.on('close', (code) => {
        try {
          
          const parsedResults = []
          
          Readable.from([output])
                  .pipe(csv())
                  .on('data', (data) => parsedResults.push(data))
                  .on('end', () => {
                    res({ nodes: parsedResults })
                  })

        } catch (e) {
          console.error(output)
          rej(e)
        }
      })

    })

  }

}

module.exports = Simulation
