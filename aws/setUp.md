# Cardioid on AWS Walk Through (Work In Progress)
*Note: This walkthrough has the following presumptions:*<br>
*- Already selected and launched GPU instance type from AWS*<br>
*- Configured instance for proper network access*<br>
*- Secure Shell into the instance* *`ssh -i yourkey.pem ec2-user@you-ip-or-dns-address`*
<br>

```sh

yum install atlas atlas-devel blas-devel git gcc gcc-* kernel-devel-$(uname -r) lapack lapack-devel m4.x86_64 patchutils.x86_64 patch.x86_64 xorg* -y

```


<!--
```sh
yum update -y
yum install git -y
#Install the gcc compiler and the kernel headers package for the version of the kernel you are currently running.
yum install -y gcc kernel-devel-$(uname -r)
yum install gcc-gfortran -y
yum install gcc-c++ -y
yum install gcc-* -y
yum install gmake -y
#Installation of cmake 3.10 or higher is needed for cardioid on amzn linux
#cmake3 is availble but need to vet, for now steps below to grab, build, compile cmake3.10
#yum install cmake -y
#Issues with Spack and finding headers
#yum install openmpi openmpi-devel -y
yum install m4.x86_64 -y
yum install patchutils.x86_64 patch.x86_64 -y
yum install lapack lapack-devel -y
yum install atlas atlas-devel -y 
yum install blas-devel -y
yum install perl -y
#Potential need for pkg-config
yum install xorg* -y
yum install xorg-x11-drv-nouveau.x86_64 -y
#yum install pcp-pmda-nvidia-gpu.x86_64
```
-->




# Install `MPICH`
```sh
cd ~
wget http://www.mpich.org/static/downloads/3.3/mpich-3.3.tar.gz
tar -xvzf mpich-3.3.tar.gz
cd mpich-3.3
./configure
make
make install
```





# Install `cmake-3.10`
```sh
cd ~
wget https://cmake.org/files/v3.10/cmake-3.10.0.tar.gz
tar -xvzf cmake-3.10.0.tar.gz
cd cmake-3.10.0/
./bootstrap
make
make install
```
<!--
# IF Installing openmpi, config PATH
```sh
vi .bashrc

#APPEND:
  PATH=/usr/lib64/openmpi/bin:$PATH
  export PATH

source .bashrc
```
-->

# Installing NVIDIA Driver on AWS GPU Linux Instances
https://docs.aws.amazon.com/AWSEC2/latest/UserGuide/install-nvidia-driver.html
```sh
yum erase nvidia cuda
aws configure
aws s3 ls --recursive s3://ec2-linux-nvidia-drivers/
aws s3 cp --recursive s3://ec2-linux-nvidia-drivers/latest/ .
```



### Disable the nouveau open source driver for NVIDIA graphics cards.

```sh
cat << EOF | sudo tee --append /etc/modprobe.d/blacklist.conf
blacklist vga16fb
blacklist nouveau
blacklist rivafb
blacklist nvidiafb
blacklist rivatv
EOF
```


### Edit the /etc/default/grub file and add the following line
```sh
vi /etc/default/grub
#Append:
GRUB_CMDLINE_LINUX="rdblacklist=nouveau"
```



### Rebuild the Grub configuration
`grub2-mkconfig -o /boot/grub2/grub.cfg`



### Run the self-install script to install the NVIDIA driver that you downloaded in the previous step
`/bin/sh ./NVIDIA-Linux-x86_64*.run`

```expect
Building kernel modules.... 100%
"Install NVIDIA's 32-bit compatibility libraries?"
 > Yes

"Would you like to run the nvidia-xconfig utility to automatically update your X configuration file so that the NVIDIA X driver will be used when you restart X?  Any pre-existing X configuration file will be backed up."  
 > Yes

"Your X configuration file has been successfully updated.  Installation of the NVIDIA Accelerated Graphics Driver for Linux-x86_64 (version: 410.92) is now complete."
 > OK
```
 
```sh
OUTPUT:
Verifying archive integrity... OK
Uncompressing NVIDIA Accelerated Graphics Driver for Linux-x86_64 410.92....................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................................

```



### Reboot the instance
`reboot`



### Confirm that the driver is functional. The response for the following command lists the installed NVIDIA driver version and details about the GPUs

`nvidia-smi -q | head`
```sh
#OUTPUT:
==============NVSMI LOG==============

Timestamp                           : Thu Mar 21 17:22:32 2019
Driver Version                      : 410.92
CUDA Version                        : 10.0

Attached GPUs                       : 1
GPU 00000000:00:1E.0
    Product Name                    : Tesla M60
```

`nvidia-smi`

```sh
+-----------------------------------------------------------------------------+
| NVIDIA-SMI 410.92       Driver Version: 410.92       CUDA Version: 10.0     |
|-------------------------------+----------------------+----------------------+
| GPU  Name        Persistence-M| Bus-Id        Disp.A | Volatile Uncorr. ECC |
| Fan  Temp  Perf  Pwr:Usage/Cap|         Memory-Usage | GPU-Util  Compute M. |
|===============================+======================+======================|
|   0  Tesla M60           On   | 00000000:00:1E.0 Off |                    0 |
| N/A   30C    P8    14W / 150W |      0MiB /  7618MiB |      0%      Default |
+-------------------------------+----------------------+----------------------+
                                                                               
+-----------------------------------------------------------------------------+
| Processes:                                                       GPU Memory |
|  GPU       PID   Type   Process name                             Usage      |
|=============================================================================|
|  No running processes found                                                 |
+-----------------------------------------------------------------------------+
```

# [G3 instances only] To enable NVIDIA GRID Virtual Applications on a G3 instance, complete the GRID activation steps in Activate NVIDIA GRID Virtual Applications (G3 Instances Only) (NVIDIA GRID Virtual Workstation is enabled by default). 
Follow steps at: https://docs.aws.amazon.com/AWSEC2/latest/UserGuide/activate_grid.html




# [P2, P3, and G3 instances] Complete the optimization steps in Optimizing GPU Settings (P2, P3, and G3 Instances) to achieve the best performance from your GPU.
Follw steps at: https://docs.aws.amazon.com/AWSEC2/latest/UserGuide/optimize_gpu.html

### Configure the GPU settings to be persistent. This command can take several minutes to run

`nvidia-persistenced`



### Disable the autoboost feature for all GPUs on the instance
Note: GPUs on P3 instances do not support autoboost.

`nvidia-smi --auto-boost-default=0`



### Set all GPU clock speeds to their maximum frequency. Use the memory and graphics clock speeds specified in the following commands.
Note: Some versions of the NVIDIA driver do not allow setting application clock speed and throw a "Setting applications clocks is not supported for GPU …" error, which you can ignore.
```sh
  # P2 instances:
    nvidia-smi -ac 2505,875
  # P3 instances:
    nvidia-smi -ac 877,1530
  # G3 instances:
    nvidia-smi -ac 2505,1177

#OUTPUT:
Applications clocks set to "(MEM 2505, SM 1177)" for GPU 00000000:00:1E.0
All done
```




# Spack & Cardioid
### Get spack
`git clone https://github.com/spack/spack.git`



### Get Spack set up
`source spack/share/spack/setup-env.sh`
<br>
<br>
OR add to .bashrc
```sh
vi .bashrc
  #APPEND:
  source spack/share/spack/setup-env.sh
source .bashrc
```

`spack compiler find`



### Get Cardioid
`git clone https://github.com/llnl/cardioid.git`



### Quick Overview of Spack Creating, Activating, Deleting Environments
https://spack.readthedocs.io/en/latest/tutorial_environments.html#spack-yaml
https://spack.readthedocs.io/en/latest/command_index.html

- **Create Environment** 
`spack env create amzn2`

- **See Environments**
`spack env list`

- **Activate an Environment** 
`spack env activate amzn2`

- **Check Current Environment** 
`spack env status`

- **List and Search Installed Packages**
`spack find`

- **Leave Current Environment**
`spack env deactivate`
or for short
`despacktivate`

- **Remove an Environment**
`spack env remove amzn2`

- **View Environment Configuration** 
`spack config get`

- **Edit Environment Configuration File**
`spack config edit`



### Setup `amzn2` Spack environment
`cp cardioid/arch/osx.yaml cardioid/arch/amzn2.yaml`
<br><br>`vi cardioid/arch/amzn2.yaml`

Adjust the `amzn2.yaml` file to look like the `.YAML` below if you have followed the previous steps in the guide. 
*NOTE: Perl is not added, `spac` failed to add the locally installed `PERL` due to it appending a `/bin` to the absolute path during installation* 

```yaml
# This is a Spack Environment file.
#
# It describes a set of packages to be installed, along with
# configuration settings.
spack:
  # add package specs to the `specs` list
  specs: []
  mirrors: {}
  modules:
    enable: []
  repos: []
  packages:
    mpich:
      compiler: [gcc@7.3.1]
      version: [3.3]
      paths:
        mpich@3.3%gcc@7.3.1 arch=arch=linux-amzn2-x86_64: /usr/local
      buildable: true
      providers: {}
      modules: {}
    pkg-config:
        paths:
            pkg-config@0.27.1: /usr/bin/pkg-config
        buildable: False
    openssl:
        paths:
            openssl@1.0.2: /usr/bin/openssl
        buildable: False
    cmake:
      paths:
        cmake@3.10.0: /usr/local/bin/cmake
      buildable: false
      version: []
      providers: {}
      modules: {}
      compiler: []
  config: {}
```


<!--


```yaml
# This is a Spack Environment file.
#
# It describes a set of packages to be installed, along with
# configuration settings.
spack:
  # add package specs to the `specs` list
  specs: [pkg-config, openssl, cmake, util-macros, mpich, cardioid]
  mirrors: {}
  modules:
    enable: []
  repos: []
  packages:
   # openmpi:
   #   paths:
   #     openmpi@2.1.1: /usr/lib64/openmpi/bin/mpijavac
   #   buildable: false
   #   providers: {}
   #   modules: {}
   #   version: []
   #   compiler: []
   # util-macros:
   #   paths:
   #     util-macros@1.19.0: /usr/share/util-macros
   #   buildable: false
   #   version: []
   #   providers: {}
   #   modules: {}
   #   compiler: []
    mpich:
      compiler: [gcc@7.3.1]
      version: [3.3]
      paths:
        mpich@3.3%gcc@7.3.1 arch=arch=linux-amzn2-x86_64: /usr/local
      buildable: true
      providers: {}
      modules: {}
   # mpich:
   #   paths:
   #     mpich@3.3: /usr/local/bin/
   #     buildable: false
   #   buildable: true
   #   version: []
   #   providers: {}
   #   modules: {}
   #   compiler: []
    pkg-config:
      paths:
        pkg-config@0.27.1: /usr/bin/pkg-config
      buildable: false
      version: []
      providers: {}
      modules: {}
      compiler: []
    openssl:
      paths:
        openssl@1.0.2: /usr/bin/openssl
      buildable: false
      version: []
      providers: {}
      modules: {}
      compiler: []
    #perl:
    #  paths:
    #    perl@5.16.3: /usr/bin/perl
    #  buildable: false
    #  version: []
    #  providers: {}
    #  modules: {}
    #  compiler: []
    cmake:
      paths:
        cmake@3.10.0: /usr/local/bin/cmake
      buildable: false
      version: []
      providers: {}
      modules: {}
      compiler: []
  config: {}
```

-->

### Create `amzn2` Spack environment
`spack env create amzn2 cardioid/arch/amzn2.yaml`

`spack env activate amzn2`

`spack env status`



### Show what would be installed, given a spec
`spack spec mpich`
```sh
#OUTPUT:
Input spec
--------------------------------
mpich

Concretized
--------------------------------
mpich@3.3%gcc@7.3.1 device=ch3 +hydra netmod=tcp patches=c7d4ecf865dccff5b764d9c66b6a470d11b0b1a5b4f7ad1ffa61079ad6b5dede +pci pmi=pmi +romio~slurm~verbs arch=linux-amzn2-x86_64 
```

`spack spec cmake`
```sh
Input spec
--------------------------------
cmake

Concretized
--------------------------------
cmake@3.10.0%gcc@7.3.1~doc+ncurses+openssl~ownlibs patches=dd3a40d4d92f6b2158b87d6fb354c277947c776424aa03f6dc8096cf3135f5d0 ~qt arch=linux-amzn2-x86_64 
```

`spack spec pkg-config`
```sh
#OUTPUT:
Input spec
--------------------------------
pkg-config

Concretized
--------------------------------
pkg-config@0.27.1%gcc@7.3.1+internal_glib patches=49ffcd644e190dc5efcb2fab491177811ea746c1a526f75d77118c2706574358 arch=linux-amzn2-x86_64 

```

`spack spec openssl`
```sh
#OUTPUT:
Input spec
--------------------------------
openssl

Concretized
--------------------------------
openssl@1.0.2%gcc@7.3.1+systemcerts arch=linux-amzn2-x86_64 
```



### Install Packages Individually
`spack install mpich`
```sh
#OUTPUT:
==> mpich@3.3 : externally installed in /usr/local
==> mpich@3.3 : generating module file
==> mpich@3.3 : registering into DB
```

`spack install cmake`
```sh
#OUTPUT:
==> cmake@3.10.0 : externally installed in /usr/local/bin/cmake
==> cmake@3.10.0 : generating module file
==> cmake@3.10.0 : registering into DB
```

`spack install pkg-config`
```sh
#OUTPUT:
==> pkg-config@0.27.1 : externally installed in /usr/bin/pkg-config
==> pkg-config@0.27.1 : generating module file
==> pkg-config@0.27.1 : registering into DB
```

`spack install openssl`
```sh
#OUTPUT:
==> openssl@1.0.2 : externally installed in /usr/bin/openssl
==> openssl@1.0.2 : generating module file
==> openssl@1.0.2 : registering into DB
```



### Install All Packages
`spack install`

```sh
#OUTPUT:
==> Installing environment amzn2
==> mpich@3.3 : externally installed in /usr/local
==> mpich@3.3 : already registered in DB
==> cmake@3.10.0 : externally installed in /usr/local/bin/cmake
==> cmake@3.10.0 : already registered in DB
==> pkg-config@0.27.1 : externally installed in /usr/bin/pkg-config
==> pkg-config@0.27.1 : already registered in DB
==> openssl@1.0.2 : externally installed in /usr/bin/openssl
==> openssl@1.0.2 : already registered in DB
```



### Concretize an Environment and Write a lockfile
`spack concretize -f`
```sh
#OUTPUT:
==> Concretizing mpich
[+]  ftkckrb  mpich@3.3%gcc@7.3.1 device=ch3 +hydra netmod=tcp patches=c7d4ecf865dccff5b764d9c66b6a470d11b0b1a5b4f7ad1ffa61079ad6b5dede +pci pmi=pmi +romio~slurm~verbs arch=linux-amzn2-x86_64 
==> Concretizing cmake
[+]  kijol6x  cmake@3.10.0%gcc@7.3.1~doc+ncurses+openssl~ownlibs patches=dd3a40d4d92f6b2158b87d6fb354c277947c776424aa03f6dc8096cf3135f5d0 ~qt arch=linux-amzn2-x86_64 
==> Concretizing pkg-config
[+]  wrtknyv  pkg-config@0.27.1%gcc@7.3.1+internal_glib patches=49ffcd644e190dc5efcb2fab491177811ea746c1a526f75d77118c2706574358 arch=linux-amzn2-x86_64 
==> Concretizing openssl
[+]  kzz5x3q  openssl@1.0.2%gcc@7.3.1+systemcerts arch=linux-amzn2-x86_64 
```





# Cardioid Prep and Install
`spack graph cardioid`
```sh
#OUTPUT:
o  cardioid
|\
| |\
| | |\
o | | |  perl
o | | |  gdbm
o | | |  readline
o | | |  ncurses
o | | |  pkg-config
 / / /
o | |  openblas
 / /
o |  mpich
 /
o  cmake

```

`spack install cardioid`

```sh
#OUTPUT:

==> cmake@3.10.0 : externally installed in /usr/local/bin/cmake
==> cmake@3.10.0 : already registered in DB
==> mpich@3.3 : externally installed in /usr/local
==> mpich@3.3 : already registered in DB
==> Installing openblas
==> Searching for binary cache of openblas
==> Warning: No Spack mirrors are currently configured
==> No binary for openblas found: installing from source
==> Fetching http://github.com/xianyi/OpenBLAS/archive/v0.3.5.tar.gz
######################################################################################################################################################################### 100.0%
==> Staging archive: /root/spack/var/spack/stage/openblas-0.3.5-mqotiwlie6h2d6rumhjhbf7fmqd4usjf/v0.3.5.tar.gz
==> Created stage in /root/spack/var/spack/stage/openblas-0.3.5-mqotiwlie6h2d6rumhjhbf7fmqd4usjf
==> No patches needed for openblas
==> Building openblas [MakefilePackage]
==> Executing phase: 'edit'
==> Executing phase: 'build'
==> Executing phase: 'install'
==> Successfully installed openblas
  Fetch: 0.27s.  Build: 4m 29.54s.  Total: 4m 29.81s.
[+] /root/spack/opt/spack/linux-amzn2-x86_64/gcc-7.3.1/openblas-0.3.5-mqotiwlie6h2d6rumhjhbf7fmqd4usjf
==> pkg-config@0.27.1 : externally installed in /usr/bin/pkg-config
==> pkg-config@0.27.1 : already registered in DB
==> Installing ncurses
==> Searching for binary cache of ncurses
==> Warning: No Spack mirrors are currently configured
==> No binary for ncurses found: installing from source
==> Fetching https://ftpmirror.gnu.org/ncurses/ncurses-6.1.tar.gz
######################################################################################################################################################################### 100.0%
==> Staging archive: /root/spack/var/spack/stage/ncurses-6.1-fzjg7pyd6un2n5rrrn447f665nw72rzj/ncurses-6.1.tar.gz
==> Created stage in /root/spack/var/spack/stage/ncurses-6.1-fzjg7pyd6un2n5rrrn447f665nw72rzj
==> No patches needed for ncurses
==> Building ncurses [AutotoolsPackage]
==> Executing phase: 'autoreconf'
==> Executing phase: 'configure'
==> Executing phase: 'build'
==> Executing phase: 'install'
==> Successfully installed ncurses
  Fetch: 1.23s.  Build: 1m 11.12s.  Total: 1m 12.35s.
[+] /root/spack/opt/spack/linux-amzn2-x86_64/gcc-7.3.1/ncurses-6.1-fzjg7pyd6un2n5rrrn447f665nw72rzj
==> Installing readline
==> Searching for binary cache of readline
==> Warning: No Spack mirrors are currently configured
==> No binary for readline found: installing from source
==> Fetching https://ftpmirror.gnu.org/readline/readline-7.0.tar.gz
######################################################################################################################################################################### 100.0%
==> Staging archive: /root/spack/var/spack/stage/readline-7.0-qp27l2gp6smarb5upeldly2rdehwhaig/readline-7.0.tar.gz
==> Created stage in /root/spack/var/spack/stage/readline-7.0-qp27l2gp6smarb5upeldly2rdehwhaig
==> No patches needed for readline
==> Building readline [AutotoolsPackage]
==> Executing phase: 'autoreconf'
==> Executing phase: 'configure'
==> Executing phase: 'build'
==> Executing phase: 'install'
==> Successfully installed readline
  Fetch: 0.63s.  Build: 9.00s.  Total: 9.63s.
[+] /root/spack/opt/spack/linux-amzn2-x86_64/gcc-7.3.1/readline-7.0-qp27l2gp6smarb5upeldly2rdehwhaig
==> Installing gdbm
==> Searching for binary cache of gdbm
==> Warning: No Spack mirrors are currently configured
==> No binary for gdbm found: installing from source
==> Fetching https://ftpmirror.gnu.org/gdbm/gdbm-1.18.1.tar.gz
######################################################################################################################################################################### 100.0%
==> Staging archive: /root/spack/var/spack/stage/gdbm-1.18.1-zqeyqymgebu64cdtwadljkc2gfqc47g4/gdbm-1.18.1.tar.gz
==> Created stage in /root/spack/var/spack/stage/gdbm-1.18.1-zqeyqymgebu64cdtwadljkc2gfqc47g4
==> No patches needed for gdbm
==> Building gdbm [AutotoolsPackage]
==> Executing phase: 'autoreconf'
==> Executing phase: 'configure'
==> Executing phase: 'build'
==> Executing phase: 'install'
==> Successfully installed gdbm
  Fetch: 1.18s.  Build: 8.27s.  Total: 9.46s.
[+] /root/spack/opt/spack/linux-amzn2-x86_64/gcc-7.3.1/gdbm-1.18.1-zqeyqymgebu64cdtwadljkc2gfqc47g4
==> Installing perl
==> Searching for binary cache of perl
==> Warning: No Spack mirrors are currently configured
==> No binary for perl found: installing from source
==> Fetching http://www.cpan.org/src/5.0/perl-5.26.2.tar.gz
######################################################################################################################################################################### 100.0%
==> Fetching http://search.cpan.org/CPAN/authors/id/M/MI/MIYAGAWA/App-cpanminus-1.7042.tar.gz
######################################################################################################################################################################### 100.0%
==> Fetching https://src.fedoraproject.org/rpms/perl/raw/004cea3a67df42e92ffdf4e9ac36d47a3c6a05a4/f/perl-5.26.1-guard_old_libcrypt_fix.patch
######################################################################################################################################################################### 100.0%
==> Staging archive: /root/spack/var/spack/stage/perl-5.26.2-rg4fspis3pom7rafriudir7vkbe3gn5p/perl-5.26.2.tar.gz
==> Created stage in /root/spack/var/spack/stage/perl-5.26.2-rg4fspis3pom7rafriudir7vkbe3gn5p
==> Staging archive: /root/spack/var/spack/stage/resource-cpanm-rg4fspis3pom7rafriudir7vkbe3gn5p/App-cpanminus-1.7042.tar.gz
==> Created stage in /root/spack/var/spack/stage/resource-cpanm-rg4fspis3pom7rafriudir7vkbe3gn5p
==> Moving resource stage
	source : /root/spack/var/spack/stage/resource-cpanm-rg4fspis3pom7rafriudir7vkbe3gn5p/App-cpanminus-1.7042/
	destination : /root/spack/var/spack/stage/perl-5.26.2-rg4fspis3pom7rafriudir7vkbe3gn5p/perl-5.26.2/cpanm/cpanm
==> Applied patch https://src.fedoraproject.org/rpms/perl/raw/004cea3a67df42e92ffdf4e9ac36d47a3c6a05a4/f/perl-5.26.1-guard_old_libcrypt_fix.patch
==> Building perl [Package]
==> Executing phase: 'configure'
==> Executing phase: 'build'
==> Executing phase: 'install'
==> Successfully installed perl
  Fetch: 0.94s.  Build: 2m 19.72s.  Total: 2m 20.67s.
[+] /root/spack/opt/spack/linux-amzn2-x86_64/gcc-7.3.1/perl-5.26.2-rg4fspis3pom7rafriudir7vkbe3gn5p
==> Installing cardioid
==> Searching for binary cache of cardioid
==> Warning: No Spack mirrors are currently configured
==> No binary for cardioid found: installing from source
==> Cloning git repository: https://github.com/LLNL/cardioid.git on branch elec-fem
==> No checksum needed when fetching with git
==> Already staged cardioid-elecfem-ipc4sebwrqzpcfgiaopyy6al3hvsukbb in /root/spack/var/spack/stage/cardioid-elecfem-ipc4sebwrqzpcfgiaopyy6al3hvsukbb
==> No patches needed for cardioid
==> Building cardioid [CMakePackage]
==> Executing phase: 'cmake'
==> Executing phase: 'build'
==> Executing phase: 'install'
==> Successfully installed cardioid
  Fetch: 14.64s.  Build: 45.87s.  Total: 1m 0.51s.
[+] /root/spack/opt/spack/linux-amzn2-x86_64/gcc-7.3.1/cardioid-elecfem-ipc4sebwrqzpcfgiaopyy6al3hvsukbb

```






```sh
#OUTPUT:
==> cmake@3.10.0 : externally installed in /usr/local/bin/cmake
==> cmake@3.10.0 : already registered in DB
==> mpich@3.3 : externally installed in /usr/local
==> mpich@3.3 : already registered in DB
==> openblas is already installed in /root/spack/opt/spack/linux-amzn2-x86_64/gcc-7.3.1/openblas-0.3.5-mqotiwlie6h2d6rumhjhbf7fmqd4usjf
==> pkg-config@0.27.1 : externally installed in /usr/bin/pkg-config
==> pkg-config@0.27.1 : already registered in DB
==> ncurses is already installed in /root/spack/opt/spack/linux-amzn2-x86_64/gcc-7.3.1/ncurses-6.1-fzjg7pyd6un2n5rrrn447f665nw72rzj
==> readline is already installed in /root/spack/opt/spack/linux-amzn2-x86_64/gcc-7.3.1/readline-7.0-qp27l2gp6smarb5upeldly2rdehwhaig
==> gdbm is already installed in /root/spack/opt/spack/linux-amzn2-x86_64/gcc-7.3.1/gdbm-1.18.1-zqeyqymgebu64cdtwadljkc2gfqc47g4
==> perl is already installed in /root/spack/opt/spack/linux-amzn2-x86_64/gcc-7.3.1/perl-5.26.2-rg4fspis3pom7rafriudir7vkbe3gn5p
==> Installing cardioid
==> Searching for binary cache of cardioid
==> Warning: No Spack mirrors are currently configured
==> No binary for cardioid found: installing from source
==> Cloning git repository: https://github.com/LLNL/cardioid.git on branch elec-fem
==> No checksum needed when fetching with git
==> Already staged cardioid-elecfem-ipc4sebwrqzpcfgiaopyy6al3hvsukbb in /root/spack/var/spack/stage/cardioid-elecfem-ipc4sebwrqzpcfgiaopyy6al3hvsukbb
==> No patches needed for cardioid
==> Building cardioid [CMakePackage]
==> Executing phase: 'cmake'
==> Executing phase: 'build'
==> Executing phase: 'install'
==> Successfully installed cardioid
  Fetch: 0.73s.  Build: 17.75s.  Total: 18.48s.
[+] /root/spack/opt/spack/linux-amzn2-x86_64/gcc-7.3.1/cardioid-elecfem-ipc4sebwrqzpcfgiaopyy6al3hvsukbb
```

`spack view symlink cardioid.local cardioid`
```sh
#OUTPUT:
==> Warning: [cardioid.local] Skipping external package: mpich@3.3%gcc@7.3.1 device=ch3 +hydra netmod=tcp patches=c7d4ecf865dccff5b764d9c66b6a470d11b0b1a5b4f7ad1ffa61079ad6b5dede +pci pmi=pmi +romio~slurm~verbs arch=linux-amzn2-x86_64 /ftkckrb
```

`./cardioid.local/bin/cardioid`
```sh
#OUTPUT:
Cardioid v2.0.0
Unclassified/Code in development Distribution
LLNL-CODE-764041
Do not redistribute without permission

Version info:
-------------
  hostname      ip-172-31-37-117.ec2.internal
  user          root
  svn revision: SVNVERSION
  compile time: Mar 20 2019 19:59:28
  source path:  PATH
  build target: TARGET
  build arch:   ARCH
  CXXFLAGS:     CXXFLAGS
  CFLAGS:       CFLAGS
  LDFLAGS:      LDFLAGS
_________________________________________________________________

```






# Test Cardioid

`./cardioid.local/bin/singleCell -m TT06Dev`

```sh
fCassForm = TT06
                    0   -86.709000000000003
                    1    32.309951549541026
                    2    40.135323359053494
                    3     39.03065189310675
                    4    37.995231858174144
                    5    37.200446175685464
                    6    36.556584864661062
                    7    34.923232407809699
                    8    31.715263821991613
                    9    29.321250003577809
                   10     27.66001710563582
                   11      26.4824767433152
                   12    25.635695106245656
                   13    25.023625323304614
                   14    24.581588415025411
                   15    24.263854312373986
                   16     24.03721664084317
                   17    23.877271187308459
                   18     23.76602012289824
                   19     23.69021400581271
                   20     23.64016227977185
                   21    23.608874801090035
                   22    23.591456851285233
                   23    23.584712911622717
                   24    23.586933174294909
                   25     23.59782279617237
                   26    23.618385776117464
                   27    23.650061282112194
                   28    23.692013276305968
                   29    23.738453872249952
                   30    23.781364993160924
                   31    23.815490724547978
                   32    23.839102678398401
                   33    23.852387777667495
                   34    23.856235371667125
                   35    23.851707312806678
                   36    23.839843962341163
                   37    23.821600527955084
                   38    23.797831827152891
                   39    23.769294568078987
                   40    23.736655359502937
                   41    23.700499975313573
                   42    23.661342258822753
                   43    23.619632169795178
                   44    23.575762899896304
                   45    23.530077128150833
                   46    23.482872526119433
                   47    23.434406621142301
                   48     23.38490111228791
                   49    23.334545717895242
                   50    23.283501619351728
                   51    23.231904553907565
                   52    23.179867599829656
                   53    23.127483689693275
                   54    23.074827881702738
                   55    23.021959414276299
                   56    22.968923565441507
                   57    22.915753335643593
                   58    22.862470970199578
                   59    22.809089335704023
                   60    22.755613163109683
                   61     22.70204016889053
                   62    22.648362064588689
                   63    22.594565464105525
                   64    22.540632697286192
                   65    22.486542537640236
                   66    22.432270851417726
                   67    22.377791174704125
                   68     22.32307522469749
                   69    22.268093350876306
                   70    22.212814931350145
                   71    22.157208719300801
                   72    22.101243144065506
                   73    22.044886571081896
                   74    21.988107524603549
                   75    21.930874876804033
                   76     21.87315800661402
                   77    21.814926931378583
                   78    21.756152414180203
                   79    21.696806049445357
                   80    21.636860329238583
                   81    21.576288692447225
                   82    21.515065558871356
                   83    21.453166350057121
                   84    21.390567498546549
                   85    21.327246447062674
                   86    21.263181639005914
                   87    21.198352501503354
                   88    21.132739422130005
                   89    21.066323720305654
                   90    20.999087614266031
                   91    20.931014184409186
                   92    20.862087333728997
                   93    20.792291745965542
                   94    20.721612842027231
                   95    20.650036735171383
                   96    20.577550185367759
                   97    20.504140553213293
                   98    20.429795753715158
                   99    20.354504210213481
                  100     20.27825480867353
                  101    20.201036852540248
                  102    20.122840018314566
                  103    20.043654311981683
                  104    19.963470026394855
                  105    19.882277699695287
                  106    19.800068074827891
                  107    19.716832060194857
                  108    19.632560691473159
                  109    19.547245094608105
                  110    19.460876449983488
                  111    19.373445957758488
                  112    19.284944804352396
                  113     19.19536413005132
                  114    19.104694997704055
                  115    19.012928362469591
                  116    18.920055042574113
                  117    18.826065691031669
                  118    18.730950768280138
                  119    18.634700515681736
                  120    18.537304929835571
                  121    18.438753737648568
                  122    18.339036372110307
                  123    18.238141948716802
                  124    18.136059242487853
                  125    18.032776665522849
                  126    17.928282245040116
                  127    17.822563601844905
                  128    17.715607929172247
                  129    17.607401971850834
                  130    17.497932005735112
                  131    17.387183817353677
                  132    17.275142683721999
                  133    17.161793352269438
                  134    17.047120020830118
                  135    16.931106317648847
                  136    16.813735281353445
                  137    16.694989340845918
                  138     16.57485029506541
                  139    16.453299292576506
                  140    16.330316810937298
                  141    16.205882635801874
                  142    16.079975839712848
                  143    15.952574760539907
                  144    15.823656979520829
                  145    15.693199298862094
                  146    15.561177718856811
                  147    15.427567414478158
                  148    15.292342711407162
                  149    15.155477061454549
                  150     15.01694301733693
                  151    14.876712206768747
                  152    14.734755305832403
                  153     14.59104201159033
                  154      14.4455410139043
                  155    14.298219966429237
                  156    14.149045456750969
                  157    13.997982975640065
                  158    13.844996885397221
                  159    13.690050387269467
                  160    13.533105487921073
                  161    13.374122964948535
                  162    13.213062331435525
                  163    13.049881799551221
                  164    12.884538243204467
                  165    12.716987159776757
                  166    12.547182630969074
                  167    12.375077282812175
                  168    12.200622244906024
                  169    12.023767108973455
                  170    11.844459886834711
                  171    11.662646967934826
                  172     11.47827307658436
                  173     11.29128122910641
                  174    11.101612691119737
                  175    10.909206935229216
                  176    10.714001599441078
                  177    10.515932446671998
                  178    10.314933325778085
                  179    10.110936134591977
                  180    9.9038707855240364
                  181    9.6936651743559352
                  182    9.4802451529315164
                  183    9.2635345065290196
                  184    9.0434549367790797
                  185    8.8199260510710396
                  186    8.5928653594631363
                  187    8.3621882801741538
                  188     8.127808154779455
                  189    7.8896362742534683
                  190    7.6475819169841275
                  191    7.4015523998178727
                  192     7.151453143061393
                  193    6.8971877501479817
                  194    6.6386581023493196
                  195    6.3757644684514787
                  196    6.1084056286866861
                  197    5.8364790113883522
                  198    5.5598808397823545
                  199    5.2785062850105611
                  200    4.9922496198747686
                  201    4.7010043658704079
                  202    4.4046634238430045
                  203    4.1031191760599723
                  204    3.7962635446865152
                  205    3.4839879886637268
                  206    3.1661834179278299
                  207    2.8427400009491368
                  208    2.5135468389244591
                  209    2.1784914778902933
                  210    1.8374592288345315
                  211    1.4903322658832039
                  212    1.1369884741221001
                  213   0.77730002182126434
                  214   0.41113163690480442
                  215  0.038338574448530574
                  216  -0.34123572938252134
                  217  -0.72776231159183991
                  218   -1.1214296287085666
                  219     -1.52244676831863
                  220   -1.9310468886576737
                  221   -2.3474908403141455
                  222   -2.7720709171612254
                  223   -3.2051146789380738
                  224   -3.6469887854441514
                  225   -4.0981027817628313
                  226    -4.558912774632808
                  227   -5.0299249411131468
                  228   -5.5116988109439715
                  229   -6.0048502623379854
                  230   -6.5100541662625417
                  231   -7.0280466056851258
                  232   -7.5596265831488338
                  233   -8.1056571122112988
                  234   -8.6670655660271567
                  235   -9.2448431306308851
                  236   -9.8400431830312698
                  237   -10.453778387785871
                  238   -11.087216284172488
                  239   -11.741573124628136
                  240   -12.418105730388403
                  241   -13.118101160107706
                  242   -13.842864050392501
                  243   -14.593701592334812
                  244   -15.371906262563737
                  245   -16.178736634907324
                  246   -17.015396857528781
                  247   -17.883015680102343
                  248   -18.782626235658981
                  249   -19.715148090674216
                  250   -20.681373333993307
                  251   -21.681958634654368
                  252    -22.71742521740665
                  253   -23.788168551198478
                  254   -24.894479209302819
                  255   -26.036575856214984
                  256   -27.214650690062061
                  257   -28.428926985928396
                  258   -29.679727720036421
                  259   -30.967553672634217
                  260   -32.293168944089345
                  261   -33.657691457913138
                  262   -35.062685676977409
                  263   -36.510254238417559
                  264   -38.003124202107514
                  265   -39.544721611740343
                  266    -41.13922434658646
                  267   -42.791576721027269
                  268   -44.507438246939437
                  269   -46.293022070677374
                  270   -48.154753670330258
                  271   -50.098648390041262
                  272   -52.129275059289832
                  273   -54.248166336930211
                  274   -56.451606703488906
                  275   -58.727955234416115
                  276    -61.05509658764516
                  277    -63.39915138003871
                  278   -65.715801385789277
                  279   -67.954935091403854
                  280   -70.067760856780382
                  281     -72.0140638258911
                  282   -73.767149139444413
                  283   -75.315301007513156
                  284   -76.660171427965537
                  285   -77.813390207394946
                  286   -78.792729608175321
                  287   -79.618743435175602
                  288   -80.312301889093959
                  289   -80.893058163296473
                  290   -81.378670470549409
                  291   -81.784539983880805
                  292   -82.123851606724571
                  293   -82.407764383356863
                  294   -82.645657030590201
                  295   -82.845377733040507
                  296   -83.013474840313975
                  297   -83.155400284641331
                  298   -83.275684903760208
                  299   -83.378087885073029
                  300   -83.465723452286142
                  301   -83.541167916323403
                  302   -83.606549904321028
                  303   -83.663626216318335
                  304   -83.713845425182797
                  305   -83.758401048570093
                  306   -83.798275876789006
                  307   -83.834278827958627
                  308    -83.86707551458764
                  309   -83.897213539285019
                  310   -83.925143389342267
                  311   -83.951235669038766
                  312   -83.975795293684797
                  313   -83.999073169605722
                  314   -84.021275798315358
                  315   -84.042573169754391
                  316    -84.06310524730813
                  317   -84.082987295014391
                  318   -84.102314253607673
                  319   -84.121164335601605
                  320   -84.139601979383926
                  321   -84.157680277303243
                  322   -84.175442972116258
                  323   -84.192926099200378
                  324   -84.210159337999201
                  325   -84.227167124728922
                  326   -84.243969568995198
                  327   -84.260583209284889
                  328   -84.277021636001294
                  329   -84.293296005556471
                  330   -84.309415464813284
                  331    -84.32538750171183
                  332   -84.341218235083247
                  333   -84.356912654334067
                  334   -84.372474817782646
                  335   -84.387908016869659
                  336   -84.403214912185362
                  337   -84.418397646208135
                  338   -84.433457936785061
                  339   -84.448397154678005
                  340   -84.463216387916603
                  341   -84.477916495220867
                  342   -84.492498150360973
                  343   -84.506961878999178
                  344   -84.521308089289832
                  345   -84.535537097293499
                  346   -84.549649148080192
                  347   -84.563644433245102
                  348   -84.577523105437677
                  349   -84.591285290401927
                  350   -84.604931096940604
                  351   -84.618460625147208
                  352   -84.631873973190196
                  353   -84.645171242887116
                  354   -84.658352544265696
                  355   -84.671417999276329
                  356   -84.684367744792837
                  357   -84.697201935016196
                  358   -84.709920743376273
                  359   -84.722524364011349
                  360   -84.735013012892537
                  361   -84.747386928648638
                  362   -84.759646373138779
                  363   -84.771791631811496
                  364   -84.783823013884628
                  365   -84.795740852372489
                  366   -84.807545503985395
                  367   -84.819237348920353
                  368   -84.830816790560363
                  369    -84.84228425509697
                  370   -84.853640191087948
                  371    -84.86488506896012
                  372   -84.876019380467895
                  373   -84.887043638113227
                  374   -84.897958374535364
                  375    -84.90876414187494
                  376   -84.919461511118243
                  377    -84.93005107142551
                  378   -84.940533429447257
                  379   -84.950909208631998
                  380   -84.961179048528152
                  381   -84.971343604083003
                  382    -84.98140354494042
                  383   -84.991359554740399
                  384   -85.001212330421012
                  385   -85.010962581525092
                  386   -85.020611029513177
                  387   -85.030158407083661
                  388   -85.039605457500841
                  389   -85.048952933933279
                  390    -85.05820159880173
                  391   -85.067352223138641
                  392   -85.076405585959222
                  393   -85.085362473644793
                  394   -85.094223679339535
                  395   -85.102990002360215
                  396   -85.111662247619961
                  397   -85.120241225066124
                  398   -85.128727749132509
                  399   -85.137122638206307
                  400   -85.145426714110016
                  401   -85.153640801598016
                  402   -85.161765727868328
                  403   -85.169802322089851
                  404   -85.177751414944098
                  405   -85.185613838182519
                  406   -85.193390424198554
                  407   -85.201082005615064
                  408   -85.208689414886308
                  409     -85.2162134839148
                  410   -85.223655043682797
                  411   -85.231014923898286
                  412   -85.238293952654743
                  413   -85.245492956105878
                  414   -85.252612758153063
                  415   -85.259654180147294
                  416   -85.266618040603916
                  417   -85.273505154930461
                  418   -85.280316335167853
                  419   -85.287052389743167
                  420   -85.293714123235475
                  421   -85.300302336153308
                  422   -85.306817824723765
                  423   -85.313261380693021
                  424   -85.319633791137804
                  425   -85.325935838287904
                  426    -85.33216829935877
                  427   -85.338331946394561
                  428    -85.34442754612077
                  429   -85.350455859806658
                  430   -85.356417643136652
                  431   -85.362313646091167
                  432   -85.368144612835195
                  433   -85.373911281616259
                  434    -85.37961438466958
                  435   -85.385254648131721
                  436   -85.390832791960975
                  437   -85.396349529865745
                  438   -85.401805569239286
                  439   -85.407201611101414
                  440   -85.412538350046361
                  441   -85.417816474197252
                  442   -85.423036665165853
                  443   -85.428199598018622
                  444   -85.433305941247582
                  445   -85.438356356746951
                  446   -85.443351499794247
                  447   -85.448292019036458
                  448   -85.453178556480481
                  449    -85.45801174748803
                  450   -85.462792220774702
                  451   -85.467520598412847
                  452   -85.472197495838202
                  453   -85.476823521859927
                  454   -85.481399278674274
                  455   -85.485925361881101
                  456   -85.490402360503793
                  457   -85.494830857011692
                  458   -85.499211427345386
                  459   -85.503544640944511
                  460    -85.50783106077796
                  461   -85.512071243376312
                  462   -85.516265738866593
                  463   -85.520415091008587
                  464   -85.524519837233584
                  465   -85.528580508684314
                  466    -85.53259763025703
                  467   -85.536571720644773
                  468   -85.540503292381985
                  469   -85.544392851890905
                  470   -85.548240899528636
                  471   -85.552047929635776
                  472   -85.555814430585784
                  473   -85.559540884835357
                  474   -85.563227768975665
                  475   -85.566875553784612
                  476   -85.570484704279352
                  477   -85.574055679769913
                  478   -85.577588933913106
                  479   -85.581084914766834
                  480   -85.584544064845389
                  481   -85.587966821174447
                  482   -85.591353615346733
                  483   -85.594704873578095
                  484   -85.598021016763468
                  485   -85.601302460533191
                  486   -85.604549615309594
                  487   -85.607762886363375
                  488   -85.610942673869985
                  489   -85.614089372966646
                  490    -85.61720337380838
                  491   -85.620285061624713
                  492   -85.623334816776136
                  493   -85.626353014809979
                  494   -85.629340026516829
                  495   -85.632296217986053
                  496   -85.635221950661688
                  497   -85.638117581397793
                  498   -85.640983462513518
                  499    -85.64381994184798
                  500   -85.646627362814826
                  501   -85.649406064456471
                  502   -85.652156381497903
                  503   -85.654878644400341
                  504   -85.657573179414214
                  505   -85.660240308631913
                  506   -85.662880350040339
                  507   -85.665493617572707
                  508   -85.668080421160056
                  509   -85.670641066782281
                  510   -85.673175856518867
                  511   -85.675685088599081
                  512   -85.678169057451598
                  513   -85.680628053753736
                  514   -85.683062364480435
                  515   -85.685472272952353
                  516   -85.687858058883734
                  517   -85.690219998429768
                  518   -85.692558364233491
                  519   -85.694873425471911
                  520   -85.697165447902051
                  521   -85.699434693906312
                  522    -85.70168142253705
                  523    -85.70390588956127
                  524   -85.706108347504113
                  525   -85.708289045692595
                  526   -85.710448230297985
                  527   -85.712586144378406
                  528   -85.714703027920606
                  529   -85.716799117881294
                  530   -85.718874648227882
                  531   -85.720929849978845
                  532    -85.72296495124354
                  533   -85.724980177261486
                  534   -85.726975750441341
                  535   -85.728951890399088
                  536   -85.730908813995953
                  537   -85.732846735375773
                  538   -85.734765866001922
                  539    -85.73666641469363
                  540   -85.738548587661967
                  541   -85.740412588545283
                  542   -85.742258618444225
                  543   -85.744086875956427
                  544   -85.745897557210242
                  545   -85.747690855898583
                  546   -85.749466963311988
                  547   -85.751226068371452
                  548   -85.752968357660507
                  549   -85.754694015457389
                  550   -85.756403223766071
                  551   -85.758096162347329
                  552   -85.759773008749377
                  553   -85.761433938337888
                  554   -85.763079124325799
                  555   -85.764708737802493
                  556   -85.766322947762674
                  557   -85.767921921134985
                  558   -85.769505822809847
                  559    -85.77107481566739
                  560   -85.772629060604459
                  561   -85.774168716561832
                  562   -85.775693940550539
                  563   -85.777204887678067
                  564   -85.778701711174151
                  565   -85.780184562416224
                  566   -85.781653590954292
                  567   -85.783108944535897
                  568   -85.784550769130249
                  569   -85.785979208952298
                  570   -85.787394406486371
                  571   -85.788796502509427
                  572   -85.790185636114202
                  573   -85.791561944731797
                  574   -85.792925564153705
                  575   -85.794276628554201
                  576   -85.795615270511888
                  577   -85.796941621030911
                  578   -85.798255809562349
                  579   -85.799557964024714
                  580   -85.800848210824356
                  581    -85.80212667487595
                  582   -85.803393479621931
                  583   -85.804648747052468
                  584   -85.805892597724551
                  585   -85.807125150781133
                  586   -85.808346523969689
                  587   -85.809556833660793
                  588   -85.810756194866514
                  589    -85.81194472125803
                  590   -85.813122525183559
                  591   -85.814289717685625
                  592   -85.815446408518341
                  593     -85.8165927061643
                  594   -85.817728717851239
                  595   -85.818854549568556
                  596   -85.819970306083363
                  597   -85.821076090956652
                  598   -85.822172006558887
                  599   -85.823258154085565
                  600   -85.824334633572519
                  601   -85.825401543910871
                  602    -85.82645898286215
                  603   -85.827507047072643
                  604   -85.828545832087869
                  605    -85.82957543236698
                  606   -85.830595941296707
                  607   -85.831607451204931
                  608   -85.832610053374594
                  609   -85.833603838057059
                  610   -85.834588894485265
                  611   -85.835565310886636
                  612   -85.836533174496253
                  613   -85.837492571569172
                  614   -85.838443587393385
                  615   -85.839386306301478
                  616   -85.840320811683299
                  617   -85.841247185997659
                  618   -85.842165510784199
                  619   -85.843075866675051
                  620   -85.843978333406156
                  621   -85.844872989828573
                  622   -85.845759913919807
                  623   -85.846639182794547
                  624   -85.847510872715702
                  625   -85.848375059104782
                  626   -85.849231816552717
                  627    -85.85008121883007
                  628   -85.850923338897175
                  629   -85.851758248914436
                  630   -85.852586020252147
                  631   -85.853406723500143
                  632   -85.854220428477646
                  633   -85.855027204242859
                  634   -85.855827119102116
                  635   -85.856620240619279
                  636   -85.857406635624855
                  637   -85.858186370224971
                  638   -85.858959509810319
                  639   -85.859726119064874
                  640   -85.860486261974501
                  641   -85.861240001835384
                  642   -85.861987401262709
                  643    -85.86272852219858
                  644   -85.863463425920401
                  645   -85.864192173049091
                  646   -85.864914823556489
                  647   -85.865631436773768
                  648   -85.866342071398762
                  649   -85.867046785503874
                  650   -85.867745636543262
                  651   -85.868438681360701
                  652   -85.869125976196401
                  653   -85.869807576694555
                  654   -85.870483537910289
                  655   -85.871153914316878
                  656    -85.87181875981247
                  657   -85.872478127726936
                  658   -85.873132070828632
                  659   -85.873780641331152
                  660   -85.874423890899706
                  661   -85.875061870657731
                  662    -85.87569463119317
                  663   -85.876322222564866
                  664   -85.876944694308662
                  665   -85.877562095443665
                  666   -85.878174474478087
                  667   -85.878781879415527
                  668     -85.8793843577605
                  669   -85.879981956524546
                  670   -85.880574722231714
                  671   -85.881162700924392
                  672   -85.881745938168805
                  673   -85.882324479060628
                  674   -85.882898368230272
                  675   -85.883467649848299
                  676   -85.884032367630795
                  677   -85.884592564844567
                  678   -85.885148284312208
                  679    -85.88569956841711
                  680   -85.886246459108861
                  681   -85.886788997907814
                  682   -85.887327225910113
                  683   -85.887861183792523
                  684   -85.888390911817311
                  685   -85.888916449836842
                  686   -85.889437837298061
                  687   -85.889955113247481
                  688   -85.890468316335244
                  689   -85.890977484819842
                  690   -85.891482656572634
                  691   -85.891983869082026
                  692   -85.892481159457702
                  693   -85.892974564434979
                  694   -85.893464120379264
                  695   -85.893949863289492
                  696   -85.894431828802851
                  697   -85.894910052198597
                  698   -85.895384568402065
                  699   -85.895855411988421
                  700   -85.896322617186769
                  701   -85.896786217884028
                  702   -85.897246247628274
                  703   -85.897702739633147
                  704   -85.898155726780899
                  705   -85.898605241626512
                  706   -85.899051316400943
                  707   -85.899493983015034
                  708   -85.899933273062743
                  709   -85.900369217824661
                  710   -85.900801848271485
                  711   -85.901231195067368
                  712   -85.901657288573332
                  713   -85.902080158850524
                  714   -85.902499835663363
                  715   -85.902916348482819
                  716   -85.903329726489815
                  717   -85.903739998578047
                  718   -85.904147193357204
                  719   -85.904551339156029
                  720   -85.904952464025428
                  721   -85.905350595741297
                  722   -85.905745761807637
                  723   -85.906137989459253
                  724   -85.906527305665037
                  725   -85.906913737130296
                  726   -85.907297310299953
                  727   -85.907678051361188
                  728   -85.908055986246126
                  729   -85.908431140634704
                  730   -85.908803539957233
                  731   -85.909173209397025
                  732   -85.909540173893134
                  733   -85.909904458142748
                  734   -85.910266086604025
                  735   -85.910625083498218
                  736   -85.910981472812651
                  737   -85.911335278302829
                  738   -85.911686523494964
                  739   -85.912035231688364
                  740   -85.912381425957932
                  741   -85.912725129156428
                  742   -85.913066363916712
                  743    -85.91340515265415
                  744   -85.913741517568866
                  745   -85.914075480648052
                  746   -85.914407063667966
                  747   -85.914736288196281
                  748   -85.915063175594383
                  749   -85.915387747019125
                  750   -85.915710023425291
                  751   -85.916030025567636
                  752   -85.916347774002872
                  753   -85.916663289091588
                  754   -85.916976591000505
                  755   -85.917287699704431
                  756   -85.917596634988044
                  757   -85.917903416448027
                  758   -85.918208063494987
                  759   -85.918510595355272
                  760   -85.918811031072963
                  761   -85.919109389511561
                  762   -85.919405689356054
                  763   -85.919699949114616
                  764   -85.919992187120272
                  765   -85.920282421533059
                  766   -85.920570670341448
                  767   -85.920856951364087
                  768   -85.921141282251781
                  769   -85.921423680488942
                  770   -85.921704163395404
                  771   -85.921982748128002
                  772   -85.922259451682336
                  773   -85.922534290894163
                  774   -85.922807282441369
                  775   -85.923078442845238
                  776   -85.923347788472114
                  777   -85.923615335535132
                  778     -85.9238811000954
                  779   -85.924145098063903
                  780   -85.924407345202724
                  781   -85.924667857126678
                  782   -85.924926649304709
                  783   -85.925183737061474
                  784   -85.925439135578529
                  785    -85.92569285989606
                  786   -85.925944924914077
                  787   -85.926195345393808
                  788   -85.926444135959201
                  789   -85.926691311098281
                  790   -85.926936885164267
                  791   -85.927180872377249
                  792   -85.927423286825217
                  793   -85.927664142465531
                  794   -85.927903453126191
                  795   -85.928141232507002
                  796   -85.928377494180936
                  797   -85.928612251595325
                  798   -85.928845518073274
                  799   -85.929077306814563
                  800   -85.929307630897114
                  801   -85.929536503278086
                  802   -85.929763936795027
                  803    -85.92998994416719
                  804   -85.930214537996491
                  805   -85.930437730768801
                  806   -85.930659534854982
                  807   -85.930879962512094
                  808   -85.931099025884535
                  809   -85.931316737005005
                  810   -85.931533107795573
                  811   -85.931748150068898
                  812    -85.93196187552924
                  813   -85.932174295773549
                  814   -85.932385422292342
                  815   -85.932595266470869
                  816   -85.932803839590122
                  817    -85.93301115282793
                  818   -85.933217217259653
                  819   -85.933422043859537
                  820   -85.933625643501415
                  821    -85.93382802695983
                  822   -85.934029204910985
                  823   -85.934229187933525
                  824   -85.934427986509746
                  825   -85.934625611026249
                  826   -85.934822071775017
                  827   -85.935017378954214
                  828   -85.935211542669265
                  829    -85.93540457293355
                  830   -85.935596479669428
                  831   -85.935787272708993
                  832   -85.935976961795063
                  833   -85.936165556581798
                  834   -85.936353066635846
                  835   -85.936539501436954
                  836   -85.936724870378981
                  837   -85.936909182770492
                  838   -85.937092447835738
                  839   -85.937274674715397
                  840   -85.937455872467297
                  841   -85.937636050067454
                  842   -85.937815216410542
                  843   -85.937993380310814
                  844   -85.938170550502875
                  845   -85.938346735642284
                  846   -85.938521944306629
                  847   -85.938696184995933
                  848   -85.938869466133511
                  849   -85.939041796066732
                  850   -85.939213183067537
                  851   -85.939383635333527
                  852   -85.939553160988339
                  853   -85.939721768082421
                  854   -85.939889464593804
                  855   -85.940056258428683
                  856   -85.940222157422056
                  857   -85.940387169338564
                  858   -85.940551301873171
                  859   -85.940714562651394
                  860    -85.94087695923055
                  861   -85.941038499099903
                  862   -85.941199189681583
                  863     -85.9413590383311
                  864    -85.94151805233804
                  865   -85.941676238926661
                  866   -85.941833605256292
                  867   -85.941990158422385
                  868    -85.94214590545667
                  869   -85.942300853328064
                  870   -85.942455008942886
                  871   -85.942608379145966
                  872   -85.942760970720713
                  873   -85.942912790389926
                  874   -85.943063844816379
                  875   -85.943214140603288
                  876    -85.94336368429498
                  877   -85.943512482377258
                  878   -85.943660541278092
                  879   -85.943807867368093
                  880   -85.943954466961017
                  881   -85.944100346314443
                  882   -85.944245511630157
                  883   -85.944389969054527
                  884   -85.944533724679346
                  885   -85.944676784542139
                  886   -85.944819154626629
                  887   -85.944960840863317
                  888   -85.945101849130054
                  889   -85.945242185252297
                  890   -85.945381855003831
                  891   -85.945520864106996
                  892    -85.94565921823326
                  893   -85.945796923004025
                  894   -85.945933983990471
                  895   -85.946070406714384
                  896   -85.946206196648703
                  897   -85.946341359217669
                  898   -85.946475899797477
                  899   -85.946609823716685
                  900   -85.946743136256501
                  901   -85.946875842651551
                  902   -85.947007948090018
                  903   -85.947139457714002
                  904   -85.947270376620182
                  905   -85.947400709860133
                  906   -85.947530462440753
                  907   -85.947659639324726
                  908   -85.947788245430672
                  909   -85.947916285633866
                  910    -85.94804376476651
                  911   -85.948170687617989
                  912   -85.948297058935424
                  913   -85.948422883424101
                  914   -85.948548165747752
                  915   -85.948672910528842
                  916   -85.948797122349163
                  917   -85.948920805749978
                  918   -85.949043965232661
                  919   -85.949166605258782
                  920   -85.949288730250601
                  921   -85.949410344591413
                  922   -85.949531452626005
                  923   -85.949652058660647
                  924   -85.949772166963925
                  925   -85.949891781766809
                  926   -85.950010907262993
                  927   -85.950129547609208
                  928   -85.950247706925765
                  929   -85.950365389296564
                  930   -85.950482598769852
                  931   -85.950599339358035
                  932   -85.950715615038433
                  933   -85.950831429753265
                  934   -85.950946787410288
                  935   -85.951061691882856
                  936   -85.951176147010273
                  937    -85.95129015659829
                  938   -85.951403724419208
                  939   -85.951516854212102
                  940   -85.951629549683318
                  941   -85.951741814506761
                  942   -85.951853652324161
                  943   -85.951965066745188
                  944   -85.952076061348009
                  945   -85.952186639679383
                  946   -85.952296805255003
                  947   -85.952406561559812
                  948   -85.952515912048085
                  949   -85.952624860144084
                  950   -85.952733409241901
                  951   -85.952841562706169
                  952   -85.952949323871877
                  953   -85.953056696044897
                  954   -85.953163682502222
                  955   -85.953270286492213
                  956   -85.953376511234822
                  957   -85.953482359921892
                  958   -85.953587835717244
                  959   -85.953692941757183
                  960   -85.953797681150675
                  961   -85.953902056979473
                  962   -85.954006072298398
                  963   -85.954109730135642
                  964    -85.95421303349292
                  965   -85.954315985345886
                  966   -85.954418588644216
                  967   -85.954520846311624
                  968   -85.954622761246654
                  969   -85.954724336322386
                  970   -85.954825574386987
                  971   -85.954926478263644
                  972   -85.955027050751184
                  973   -85.955127294623779
                  974   -85.955227212631598
                  975   -85.955326807500924
                  976   -85.955426081934149
                  977   -85.955525038610261
                  978     -85.9556236801849
                  979    -85.95572200929054
                  980   -85.955820028536834
                  981   -85.955917740510742
                  982    -85.95601514777664
                  983   -85.956112252876764
                  984    -85.95620905833114
                  985   -85.956305566637823
                  986   -85.956401780273353
                  987   -85.956497701692541
                  988   -85.956593333329124
                  989   -85.956688677595452
                  990   -85.956783736883139
                  991   -85.956878513562827
                  992   -85.956973009984736
                  993   -85.957067228478692
                  994   -85.957161171354215
                  995   -85.957254840900887
                  996   -85.957348239388381
                  997   -85.957441369066601
                  998   -85.957534232166012
                  999   -85.957626830897809
                 1000   -85.957719167453959
```
