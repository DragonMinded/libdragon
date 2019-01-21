const { exec } = require('child_process');

const options = {
  PROJECT_NAME: process.argv[2] || 'libdragon',
  BYTE_SWAP: false
}

process.argv.forEach(function (val) {
  if (val === '--byte-swap') {
    options.BYTE_SWAP = true;
  }
});

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
  await runCommand('docker run --name=' + options.PROJECT_NAME + (options.BYTE_SWAP ? ' -e N64_BYTE_SWAP=true' : '') + ' -d --mount type=bind,source="' + process.cwd() + '",target=/' + options.PROJECT_NAME + ' -w="/' + options.PROJECT_NAME + '" anacierdem/libdragon tail -f /dev/null');
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
    await runCommand('docker start ' + options.PROJECT_NAME);
    await runCommand('docker exec ' + options.PROJECT_NAME + ' make ' + param);
  },
  stop: async function stop() {
    const list = await runCommand('docker ps -a -q -f name=' + options.PROJECT_NAME);
    if (!list) {
      await Promise.reject('Toolchain is not running.');
    }
    await runCommand('docker rm -f ' + options.PROJECT_NAME);
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
      process.exit(0);
    }).catch((e) => {
      console.error(e);
      process.exit(1);
    });
  }
});