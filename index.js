#!/usr/bin/env node

const { exec } = require('child_process');
const path = require('path');
const _ = require('lodash');
const { version } = require('./package.json'); // Always use self version for docker image

// Default options
const options = {
  PROJECT_NAME: process.env.npm_package_name || 'libdragon', // Use active package name when available
  BYTE_SWAP: false,
  MOUNT_PATH: process.cwd()
}

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
  const containerID = await runCommand('docker container ls -q -f name=' + options.PROJECT_NAME);
  if (containerID) {
    await runCommand('docker container rm -f ' + containerID);
  }
  await runCommand('docker run --name=' + options.PROJECT_NAME + (options.BYTE_SWAP ? ' -e N64_BYTE_SWAP=true' : '') + ' -d --mount type=bind,source="' + options.MOUNT_PATH + '",target=/' + options.PROJECT_NAME + ' -w="/' + options.PROJECT_NAME + '" anacierdem/libdragon:' + version + ' tail -f /dev/null');
}

async function make(param) {
  await runCommand('docker start ' + options.PROJECT_NAME);
  await runCommand('docker exec ' + options.PROJECT_NAME + ' make ' + param);
}

async function download() {
  await runCommand('docker pull anacierdem/libdragon:base');
  await runCommand('docker pull anacierdem/libdragon:' + version);
  await startToolchain();
}

const availableActions = {
  start: startToolchain,
  download: download,
  init: async function initialize() {
    await runCommand('docker build -q -t anacierdem/libdragon:' + version + ' ./');
    await startToolchain();
  },
  install: async function install() {
    await download();

    const { dependencies } = require(path.join(process.cwd() + '/package.json'));

    const deps = await Promise.all(Object.keys(dependencies)
      .filter(dep => dep !== 'libdragon')
      .map(async dep => {
        const npmPath = await runCommand('npm ls ' + dep + ' --parseable=true');
        return {
          name: dep,
          paths: _.uniq(npmPath.split('\n').filter(f => f))
        }
      }));

    await Promise.all(
      deps.map(({ name, paths }) => {
        if (paths.length > 1) {
          return Promise.reject('Using same dependency with different versions is not supported!');
        }
        return new Promise(async (resolve, reject) => {
          try {
            await runCommand('docker exec ' + options.PROJECT_NAME + ' mkdir -p /' + options.PROJECT_NAME + '/.tmp/' + name + '/');
            await runCommand('docker cp ' + p + '/. ' + options.PROJECT_NAME + ':/' + options.PROJECT_NAME + '/.tmp/' + name + '/');
            await runCommand('docker exec ' + options.PROJECT_NAME + '[ -f /' + options.PROJECT_NAME + '/.tmp/' + name + '/Makefile ] &&  make -C ./.tmp/' + name + '/ && make -C ./.tmp/' + name + '/ install');
            resolve();
          } catch(e) {
            reject(e);
          }
        });
      })
    );
  },
  make: make,
  stop: async function stop() {
    const list = await runCommand('docker ps -a -q -f name=' + options.PROJECT_NAME);
    if (!list) {
      await Promise.reject('Toolchain is not running.');
    }
    await runCommand('docker rm -f ' + options.PROJECT_NAME);
  },
  // This requires docker login
  update: async function update() {
    await runCommand('docker build -q -t anacierdem/libdragon:' + version + ' -f ./update/Dockerfile ./');
    await runCommand('docker tag anacierdem/libdragon:' + version + ' anacierdem/libdragon:latest');
    await runCommand('docker push anacierdem/libdragon:' + version);
    await runCommand('docker push anacierdem/libdragon:latest');
  },
}

process.argv.forEach(function (val, index) {
  if (index < 2) {
    return;
  }

  if (val === '--byte-swap') {
    options.BYTE_SWAP = true;
    return;
  }

  if (val === '--help') {
    console.log('Available actions:');
    Object.keys(availableActions).forEach((val) => {
      console.log(val);
    });
    process.exit(0);
    return;
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