cat <<EOF > /etc/apt/sources.list.d/llvm.list
deb http://apt.llvm.org/xenial/ llvm-toolchain-xenial-6.0 main
deb-src http://apt.llvm.org/xenial/ llvm-toolchain-xenial-6.0 main
EOF
wget -O - http://apt.llvm.org/llvm-snapshot.gpg.key|sudo apt-key add -
sudo apt-get update
sudo apt-get install -y llvm-6.0 python3-dev libgc-dev libreadline-dev
pip3 install pybind11
sudo ln -s -r /usr/include/llvm-6.0/llvm /usr/include/llvm
sudo ln -s -r /usr/include/llvm-c-6.0/llvm-c/ /usr/include/llvm-c
mkdir dependency
cd dependency
git init
git remote add -f origin https://github.com/pybind/pybind11
git checkout origin/master include/pybind11
mkdir -p include/nlohmann
cd include/nlohmann
wget https://raw.githubusercontent.com/nlohmann/json/develop/single_include/nlohmann/json.hpp
wget https://raw.githubusercontent.com/nlohmann/fifo_map/master/src/fifo_map.hpp
