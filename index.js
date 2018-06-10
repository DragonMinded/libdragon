const { exec } = require('child_process');

const PROJECT_NAME = 'libdragon';

function runCommand(cmd) {
  return new Promise((resolve, reject) => {
    const command = exec(cmd, {}, (err, stdout, stderr) => {
      if (err === null) {
        console.log('Finished running ' + cmd);
        resolve(stdout);
      } else {
        reject(err);
      }
    });

    command.stdout.on('data', function (data) {
      console.log(data.toString());
    });

    command.stderr.on('data', function (data) {
      console.log(data.toString());
    });
  })
}

async function startToolchain() {
  await runCommand('docker run --name=' + PROJECT_NAME + ' -d --mount type=bind,source="' + __dirname + '",target=/' + PROJECT_NAME + ' -w="/' + PROJECT_NAME + '" anacierdem/libdragon tail -f /dev/null');
}

const availableActions = {
  start: startToolchain,
  download: async function download() {
    await runCommand('docker pull anacierdem/libdragon:latest');
    startToolchain();
  },
  init: async function initialize() {
    await runCommand('docker build -t anacierdem/libdragon:latest ./');
    startToolchain();
  },
  make: async function make(param) {
    await runCommand('docker start ' + PROJECT_NAME);
    await runCommand('docker exec ' + PROJECT_NAME + ' make ' + param);
  },
  stop: async function stop() {
    const list = await runCommand('docker ps -a -q -f name=' + PROJECT_NAME);
    if (!list) {
      await Promise.reject('Toolchain is not running.');
    }
    await runCommand('docker rm -f ' + PROJECT_NAME);
  }
}

process.argv.forEach(function (val, index, array) {
  if (val === 'help') {
    console.log('Available actions:');
    Object.keys(availableActions).forEach((val) => {
      console.log(val);
    });
    process.exit();
  }

  const functionToRun = availableActions[val];
  if (typeof functionToRun === 'function') {
    const params = process.argv.slice(index + 1);
    const param = params.join(' ');

    functionToRun(param).then((r) => {
      console.log('Completed task: ' + val);
    }).catch((e) => {
      console.error(e);
    });
  }
});

