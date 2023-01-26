const csv = require('csv-parser')
const fs = require('fs')
const converter = require('json-2-csv')
const results = []

const scenarioId = process.argv[2]

fs.createReadStream(`./scenarios/scenario_${scenarioId}_devices.csv`)
  .pipe(csv())
  .on('data', (data) => {
    
    const start = 0
    const finish = (data.deadline - 1) * data.slotlength
    
    data.delay = getRandomNumberWithPrecision(start, finish, 0.001).toFixed(3)
    
    results.push(data)
  })
  .on('end', async () => {
  
    const csvTxt = await converter.json2csvAsync(results)
    fs.writeFileSync(`./scenarios/schedules/scenario_${scenarioId}_schedule_random_sf12.csv`, csvTxt)
  })

function getRandomNumberWithPrecision(min, max, precision) {
  return Math.floor((Math.random() * (max - min + 1) + min) / precision) * precision;
}
