const fs = require('fs')
const path = require('path')

const csv = require('csv-parser')

const yaml = require('js-yaml')
const converter = require('json-2-csv')

const Scenario = require('./Scenario')
const Simulation = require('./Simulation')

const yargs = require('yargs/yargs')
const { hideBin } = require('yargs/helpers')

const flatten = require('flat')

// source: https://avbentem.github.io/airtime-calculator/ttn/eu868/51
let packetAirtimeBySF = {
  '7': 0.3689,
  '8': 0.6559,
  '9': 0.6769,
  '10': 0.6948,
  '11': 1.5606,
  '12': 2.7935
}

yargs(hideBin(process.argv))
  .command('scenarios', 'create scenarios', (yargs) => {
    return yargs
      .positional('config', {
        describe: 'path to config file',
        default: './config.yaml'
      })
  }, (argv) => {
    createScenarios(argv)
  })
  .command('run', 'run the simulations', (yargs) => {
    return yargs
      .positional('config', {
        describe: 'path to config file',
        default: './config.yaml'
      })
      .positional('mode', {
        describe: 'either rt or random',
        default: 'rt'
      })
  }, (argv) => {
    run(argv)
  })
  .option('verbose', {
    alias: 'v',
    type: 'boolean',
    description: 'Run with verbose logging',
  })
  .argv

async function createScenarios(options) {

  try {

    const config = yaml.load(fs.readFileSync(options.config, 'utf8'))
    const scenariosDir = config.scenarios.dir

    // create directory if it doesn't exist
    if (!fs.existsSync(scenariosDir)){
      fs.mkdirSync(scenariosDir);
    }

    // if there are already files in dir, do not create scenarios
    if (fs.readdirSync(scenariosDir).length > 0) {
      return
    }

    //for (const i in Array(config.scenarios.qty).fill()) {
    for (const i of [1]) {
      const iteration = parseInt(i)

      const scenario = new Scenario()
      scenario.create(config.scenarios, iteration)

      const svg = await scenario.getRenderInSVG()
      const exported = await scenario.export()
/*      
      // TODO add this as middleware
      // add deadline
      exported.devices = exported.devices.map((d) => {
        d.deadline = between(config.scenarios.devices.minDeadline, config.scenarios.devices.maxDeadline)
        return d
      })
*/
      const devicesCSV = await converter.json2csvAsync(exported.devices)
      const gatewaysCSV = await converter.json2csvAsync(exported.gateways)
      
      fs.writeFileSync(`${config.scenarios.dir}/scenario_${iteration}.svg`, svg)
      // create directory if it doesn't exist
      if (!fs.existsSync(path.join(path.resolve(), config.scenarios.dir, 'tmp'))){
        fs.mkdirSync(path.join(path.resolve(), config.scenarios.dir, 'tmp'));
      }
      fs.writeFileSync(path.join(path.resolve(), config.scenarios.dir, 'tmp', `scenario_${iteration}_devices.csv`), devicesCSV)
      fs.writeFileSync(path.join(path.resolve(), config.scenarios.dir, `scenario_${iteration}_gateways.csv`), gatewaysCSV)
    }
    
    await addSFandGWs(options)
    
    fs.rmSync(path.join(path.resolve(), config.scenarios.dir, 'tmp'), { recursive: true })
    fs.mkdirSync(path.join(path.resolve(), config.scenarios.dir, 'schedules'))

  } catch (e) {
    console.error(e);
  }

}

async function run(options) {

  try {

    const mode = options.mode === 'random' ? 3 : 2

    const config = yaml.load(fs.readFileSync(options.config, 'utf8'))
    const schedulesDir = path.join(path.resolve(), config.scenarios.dir, 'schedules')
    const scenarios = fs.readdirSync(schedulesDir)
                        .reduce((acc, file) => {
                            const prefix = file.match(/scenario_[0-9]*_schedule_sf[0-9]*/gm)
                            if (prefix !== null && acc.indexOf(prefix[0]) < 0)
                              acc.push(prefix[0])
                            return acc
                          }, [])
                          
    console.log('scenarios', scenarios)

    const defaultValues = config.simulations.defaultValues ? config.simulations.defaultValues : {}

    let results = []
    let id = 0

    const totalSimulations = scenarios.length

    for (const scenario of scenarios) {

      const schedulesPath = path.join(path.resolve(), config.scenarios.dir, 'schedules', `${scenario}.csv`)
      const gatewayStr = scenario.match(/scenario_[0-9]*/)[0]
      const gatewaysPath = path.join(path.resolve(), config.scenarios.dir, `${gatewayStr}_gateways.csv`)

      const scenarioPath = {
        devices: schedulesPath,
        gateways: gatewaysPath
      }

      console.log(`Simulation ${id+1}/${totalSimulations}`)

      const sim = new Simulation(scenarioPath, defaultValues, config.ns3)
      try {
      
        const result = await sim.run(mode)
        
        //console.log('result', result)
        //const parsedResult = parseResult(result)
        //console.log('parsedResult', parsedResult)
        
        //process.exit()
        
        const resultCSV = await converter.json2csvAsync(result)
        fs.writeFileSync(path.join(path.resolve(), config.scenarios.dir, 'schedules', `${scenario}_result.csv`), resultCSV)
        

      } catch (e) {
        console.error(e)
        process.exit()
      }

    }

  } catch (e) {
    console.error(e);
  }

}

function parseResult(data) {

  const event1Data = data.filter((d) => d.eventType === '1')
  
  for (const row of event1Data) {
    row.deviceSentTimestamp = row.timestamp
    const receivedNSRow = data.find((d) => d.eventType === '2' && d.packetUid === row.packetUid)
    const receivedGWRow = data.find((d) => d.eventType === '3' && d.packetUid === row.packetUid)
    if (receivedGWRow !== undefined) {
      row.GWReceivedTimestamp = receivedGWRow.timestamp
    } else
      row.GWReceivedTimestamp = ''
    if (receivedNSRow !== undefined)
      row.NSReceivedTimestamp = receivedNSRow.timestamp
    delete row.timestamp
    delete row.eventType
  }
  
  return event1Data

}

async function addSFandGWs(options) {

  try {

    const config = yaml.load(fs.readFileSync(options.config, 'utf8'))
    const scenarios = fs.readdirSync(config.scenarios.dir)
                        .reduce((acc, file) => {
                            const prefix = file.match(/scenario_[0-9]*/gm)
                            if (prefix !== null && acc.indexOf(prefix[0]) < 0)
                              acc.push(prefix[0])
                            return acc
                          }, [])

    const defaultValues = config.simulations.defaultValues ? config.simulations.defaultValues : {}

    let results = []
    let id = 0

    const totalSimulations = scenarios.length

    for (const scenario of scenarios) {

      const devicesTmpPath = path.join(path.resolve(), config.scenarios.dir, 'tmp', `${scenario}_devices.csv`)
      const gatewaysPath = path.join(path.resolve(), config.scenarios.dir, `${scenario}_gateways.csv`)

      const scenarioPath = {
        devices: devicesTmpPath,
        gateways: gatewaysPath
      }


      console.log(`Simulation ${id+1}/${totalSimulations}`)

      const sim = new Simulation(scenarioPath, defaultValues, config.ns3)
      try {
      
        const result = await sim.getSFandGWs()
        
        // read devices original file
        const devices = await new Promise((res, rej) => {
          const devices = []
          fs.createReadStream(devicesTmpPath)
            .pipe(csv())
            .on('data', (data) => devices.push(data))
            .on('end', () => {
              res(devices)
            })
        })
        
        // count n° of devices per gateway
        let maxDevices = {}
        
        const gateways = await new Promise((res, rej) => {
          const gateways = []
          fs.createReadStream(gatewaysPath)
            .pipe(csv())
            .on('data', (data) => gateways.push(data))
            .on('end', () => {
              res(gateways)
            })
        })
        
        for (const gw of gateways) {
          const gwDevices = result.nodes.filter((d) => gw.id === d.gatewayId)
          const sfs = gwDevices.reduce((acc, curr) => { if (acc.indexOf(curr.sf) < 0) acc.push(curr.sf); return acc; }, [])

          for (const sf of sfs) {
          
            const totalDevices = gwDevices.filter((d) => sf === d.sf).length
          
            if (maxDevices[sf] === undefined || maxDevices[sf] < totalDevices)
              maxDevices[sf] = totalDevices
              
          }
        }

        // build final devices structure
        for (const i in devices) {
          const id = devices[i].id
          const processedDevices = result.nodes.filter((d) => id === d.nodeId)
          if (processedDevices[0]) {
            const sf = processedDevices[0].sf
            const slotlength = getSlotLengthBySF(sf)
            const airtime = packetAirtimeBySF[sf]
            const dutyCycleLength = Math.ceil(airtime / (slotlength*0.01))
            const windowlength = Math.max(dutyCycleLength, maxDevices[sf])
            
            devices[i].deadline = between(1, windowlength) + 1 //between(parseInt(windowlength/2), windowlength) + 1
            devices[i].sf = sf
            devices[i].gateways = processedDevices.map((d) => d.gatewayId).join(':')
            devices[i].windowlength = windowlength
            devices[i].slotlength = slotlength
          }
        }

        const devicesCSV = await converter.json2csvAsync(devices)
        fs.writeFileSync(path.join(path.resolve(), config.scenarios.dir, `${scenario}_devices.csv`), devicesCSV)
/*        
        // TODO get x,y,z and index from devices.csv
        // TODO get index from gateways.csv
        // TODO create a new devices.csv with x,y,z and gateways and sf from result
        // TODO generate new devices.csv
        // TODO count how many devices per gateway.. where to store it? maybe as a windowlength parameter?
        // TODO add deadline? -> this should be generated in the previous script
        
        result.scenario = scenario
        result.simulationId = id++
        results.push(flatten(result, { delimiter: '_' }))
*/        
      } catch (e) {
        console.error(e)
        process.exit()
      }

    }
/*
    const resultsInCSV = await converter.json2csvAsync(results)

    fs.writeFileSync('results.csv', resultsInCSV)
*/
  } catch (e) {
    console.error(e);
  }

}

function getSlotLengthBySF(sf) {

  // slot length is time_guard + max_packet_size_airtime + RX_1_DELAY + max_packet_size_airtime + RX_2_DELAY + max_packet_size_airtime + time_guard
  // RX_1_DELAY >= max_packet_size_airtime (in case a small packet is sent, we make sure it has enough time so it doesn't collide)
  // RX_2_DELAY = RX_1_DELAY + 1s (by specification)

  const max_packet_size_airtime = packetAirtimeBySF[sf]
  const time_guard = 0.001 // 1 millisecond (arbitrary). This could be changed in function of delays in software when starting sending process
  const RX_1_DELAY = max_packet_size_airtime
  const RX_2_DELAY = RX_1_DELAY + 1
  
  const slotLength = (time_guard + max_packet_size_airtime + RX_1_DELAY + max_packet_size_airtime + RX_2_DELAY + max_packet_size_airtime + time_guard).toFixed(4) // same precission as airtime source
  
  return slotLength

}

// returns a random number between min and max
function between(min, max) {  
  return Math.floor(
    getRandomNbr() * (max - min) + min
  )
}

// random function used
function getRandomNbr() {
  return Math.random()
}
