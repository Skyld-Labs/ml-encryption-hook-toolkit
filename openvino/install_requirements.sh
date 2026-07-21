#!/bin/bash
set -e
cd ~/Downloads
curl -L https://storage.openvinotoolkit.org/repositories/openvino/packages/2024.6/linux/l_openvino_toolkit_ubuntu24_2024.6.0.17404.4c0f47d2335_x86_64.tgz -o openvino.tgz
tar -xf openvino.tgz
sudo mkdir -p /opt/intel
sudo mv l_openvino_toolkit_ubuntu24_* /opt/intel/openvino_2024.6.0
cd /opt/intel/openvino_2024.6.0/install_dependencies
sudo -E ./install_openvino_dependencies.sh
sudo apt install -y libssl-dev
