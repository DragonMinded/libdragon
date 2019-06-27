#!/usr/bin/env node

const { exec } = require('child_process');
const path = require('path');
const _ = require('lodash');
const { version } = require('./package.json'); // Always use self version for docker image

// Default options
const options = {
  PROJECT_NAME: process.env.npm_package_name || 'libdragon', // Use active package name when available
  BYTE_SWAP: false,
  MOUNT_PATH: process.cwd(),
  VERSION: version.split('.'), // libdragon version
  IS_CI: process.env.CI === 'true'
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
  // Do not try to run docker if already in container
  if (process.env.IS_DOCKER === 'true') {
    return;
  }

  const containerID = await runCommand('docker container ls -q -f name=^' + options.PROJECT_NAME + '');

  if (containerID) {
    await runCommand('docker container rm -f ' + containerID);
  }

  const versionEnv = ' -e LIBDRAGON_VERSION_MAJOR=' + options.VERSION[0]
  + ' -e LIBDRAGON_VERSION_MINOR=' + options.VERSION[1]
  + ' -e LIBDRAGON_VERSION_REVISION=' + options.VERSION[2];

  // Use only base version on CI to test make && make install only
  await runCommand('docker run --name=' + options.PROJECT_NAME
    + (options.BYTE_SWAP ? ' -e N64_BYTE_SWAP=true' : '')
    + ' -e IS_DOCKER=true'
    + (options.IS_CI ? '' : versionEnv)
    + ' -d --mount type=bind,source="' + options.MOUNT_PATH + '",target=/' + options.PROJECT_NAME
    + ' -w="/' + options.PROJECT_NAME + '" anacierdem/libdragon:' + (options.IS_CI ? 'base' : version) + ' tail -f /dev/null');
}

async function make(param) {
  // Do not try to run docker if already in container
  if (process.env.IS_DOCKER === 'true') {
    await runCommand('make ' + param);
    return;
  }
  await runCommand('docker start ' + options.PROJECT_NAME);
  await runCommand('docker exec ' + options.PROJECT_NAME + ' make ' + param);
}

async function download() {
  // Do not try to run docker if already in container
  if (process.env.IS_DOCKER === 'true') {
    return;
  }
  await runCommand('docker pull anacierdem/libdragon:base');

  // Use only base version on CI to test make && make install only
  if (!options.IS_CI) {
    await runCommand('docker pull anacierdem/libdragon:' + version);
  }

  await startToolchain();
}

async function stop() {
  // Do not try to run docker if already in container
  if (process.env.IS_DOCKER === 'true') {
    return;
  }
  const list = await runCommand('docker ps -a -q -f name=' + options.PROJECT_NAME);
  if (list) {
    await runCommand('docker rm -f ' + options.PROJECT_NAME);
  }
}

const availableActions = {
  start: startToolchain,
  download: download,
  init: async function initialize() {
    // Do not try to run docker if already in container
    if (process.env.IS_DOCKER === 'true') {
      return;
    }
    await runCommand('docker build -t anacierdem/libdragon:' + version + ' ./');
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
            const relativePath = path.relative(options.MOUNT_PATH, paths[0]).replace(new RegExp('\\' + path.sep), path.posix.sep);
            const containerPath = path.posix.join('/', options.PROJECT_NAME, relativePath, '/');
            const makePath = path.posix.join(containerPath, 'Makefile');

            // Do not try to run docker if already in container
            if (process.env.IS_DOCKER === 'true') {
              await runCommand('[ -f ' + makePath + ' ] &&  make -C ' + containerPath + ' && make -C ' + containerPath + ' install');
            } else {
              await runCommand('docker exec ' + options.PROJECT_NAME + ' /bin/bash -c "[ -f ' + makePath + ' ] &&  make -C ' + containerPath + ' && make -C ' + containerPath + ' install"');
            }
            resolve();
          } catch(e) {
            reject(e);
          }
        });
      })
    );
  },
  make: make,
  stop: stop,
  // This requires docker login
  update: async function update() {
    // Do not try to run docker if already in container
    if (process.env.IS_DOCKER === 'true') {
      return;
    }
    await stop();
    await runCommand('docker build' 
    + ' --build-arg LIBDRAGON_VERSION_MAJOR=' + options.VERSION[0]
    + ' --build-arg LIBDRAGON_VERSION_MINOR=' + options.VERSION[1]
    + ' --build-arg LIBDRAGON_VERSION_REVISION=' + options.VERSION[2]
    + ' -t anacierdem/libdragon:' + version + ' -f ./update.Dockerfile ./');
    await runCommand('docker tag anacierdem/libdragon:' + version + ' anacierdem/libdragon:latest');
    await runCommand('docker push anacierdem/libdragon:' + version);
    await runCommand('docker push anacierdem/libdragon:latest');
    await startToolchain();
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