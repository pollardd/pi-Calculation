Very Large Pi Calculations 
Version 1.1

This program calculates Pi to an arbitrary number of decimal places using either the Gauss-Legendre or Chudnovsky algorithm (default: Gauss-Legendre), with optional real-time system monitoring. At the time of writing I calculated up to 1 billion decimal places with 15GB of installed RAM however I did not reach any limits.

Features

    Calculate Pi to billions + of decimal places
    Choose calculation method: Gauss-Legendre or Chudnovsky
    Real-time CPU usage, memory usage, and CPU temperature monitoring
    Verification against known Pi reference file
    Multi-threaded system monitoring
    Debug output option

Dependencies
Build Dependencies

The following packages are required to build the program:
    g++ (C++ compiler supporting C++11 or later)
    libmpfr-dev (Multiple Precision Floating-Point Reliable library)
    libgmp-dev (GNU Multiple Precision Arithmetic Library)
    pthread (typically included by default)

Install on Debian-based systems:
    sudo apt update
    sudo apt install g++ libmpfr-dev libgmp-dev

Optional Runtime Dependency
    The following package is optional but required if you want CPU temperature monitoring:
    lm-sensors — Used to display CPU temperature in real-time monitoring output.

Install lm-sensors on Debian-based systems:
    sudo apt install lm-sensors
    sudo sensors-detect

Note:
If lm-sensors is not installed or not configured, temperature monitoring will be disabled, but Pi calculation will still run without issue.
A helpful message will notify you during execution if lm-sensors is missing.

Building the Program
Debian PC
Use the following command to compile:
    g++ -O3 calculate_pi.cpp chudnovsky.cpp -std=c++17 -o calculate_pi -lmpfr -lgmp -lm -pthread -Wall
Usage
    calculate_pi &lt;decimal_places&gt; [options]

Options:
Option	Description
  -f, --file &lt;filename&gt;        Specify reference Pi file (default ./pi_reference_1M.txt) 
  -d, --debug &lt;1|2|3&gt;          Set debug level (default: 0)
  -m, --method &lt;name&gt;          Choose method: 'gauss_legendre' (default) or 'chudnovsky'
      --threads &lt;count&gt;        Number of threads to use (valid only for chudnovsky) 1=execute in main thread, default is max -1.
      --dynamic                Share the calculations in chunks evenly between cpu cores. (valid only for chudnovsky) 
  -h, --help                   Show this help message

    
Example:
    time calculate_pi 100000 --method chudnovsky --threads 2  -d 2
    calculate pi to 100,000 decimal places using the chudnovsky formula executing using 2 sub threads with debug level 2 output
    and show total run time at the end.

Runtime Warnings
    If the program detects that sensors (from lm-sensors) is missing or not configured, it will display:
    Warning: 'sensors' command not found. CPU temperature monitoring disabled.
    See README.md for instructions to install 'lm-sensors' if desired.

    The program uses a known good value for pi to decide on success or failure of the calculation. This is specified with the -f flag.
    If not specified the program will default to ./pi_reference_1M.txt. (1 million decimal places)
    A file containing pi calculated to 1 billion or 50 billion places using the "Chudnovsky Formula" can be downloaded from this web site. 
    https://ehfd.github.io/computing/calculation-results-for-pi-up-to-50-000-000-000-digits/
        Note: The digits are released under an Attribution-NonCommercial-NoDerivatives 4.0 International License, which prohibits 
        commercial use and distribution of remixed, transformed, or built upon versions without consent. Proper attribution and 
        indication of changes are required even if it is not a prohibited use case.
        (Personaly I'm not sure how you can put a license on pi but here it is)
        The test file included is length 1,000,002 characters. Note the extra two characters are the "3." at the begining, making up 1 million decimal places.
        
Notes
    Memory Consideration:
    Calculating Pi to millions of digits requires a significant amount of memory.
    Example: For 1 million+ digits, at least 8 GB RAM is recommended although not tested.

    CPU Temperature Monitoring:
    Ensure lm-sensors is installed and configured correctly:
     
    sudo apt install lm-sensors
    sudo sensors-detect

    For further help, refer to the lm-sensors documentation.

License
    Do what ever you like but don't delete my name and call it your own.

Future Improvements
    GPU acceleration 
    Cross-platform compatibility enhancements (Mac/Windows)
    Automatic dependency checker in a setup script or Makefile (mostly complete needs more testing)

Build and Install Notes
Make sure that install.sh is executable with chmod +x install.sh

Summary of Steps
The build process is expecting a folder to already exists /usr/local/bin and be executable.
Run install.sh to build and install dependencies plus copy the executable to /usr/bin/local.

Power Monitoring Permissions
&lt;TODO&gt; Power monitoring is currently unstable and disabled.  
The fix described below worked for me for a while then stopped working.

    By default on Debian you need to have root permissions (sudo) to access the directory
    structure where the power usage information is stored otherwise you will see an error 
    message something like this.
    Error: Unable to open energy file at /sys/class/powercap/intel-rapl/intel-rapl:0/energy_uj
    The fix is a little obscure and ChatGPT described it as "A bit of an unsupported hack".
    It works, so here it is so you can decide it you use it or not.

    1. Create a config file
    sudo nano /etc/tmpfiles.d/intel-rapl.conf

    2. Add these lines to make the energy_uj files world-readable.
    f /sys/class/powercap/intel-rapl:0/energy_uj 0644 root root -
    f /sys/class/powercap/intel-rapl:0:0/energy_uj 0644 root root -
    f /sys/class/powercap/intel-rapl:0:1/energy_uj 0644 root root -
    f /sys/class/powercap/intel-rapl:0:2/energy_uj 0644 root root -

    3. Immediately apply the new config (or simply reboot) 
    sudo systemd-tmpfiles --create /etc/tmpfiles.d/intel-rapl.conf


Building on Mac
This is my experience compiling and running on a Mac with the follwoing spec.

OS:		MacOs Monterey
Hardware:	Mac Minit (Late 2014)
Processor:	Dual Core 2.6 Ghz Intel i5
RAM:		8GB (1600MHz DDR3)

ChatGPT reports the follwoing requirements for the build.

Install Xcode Command line tools
	xcode-select --install

Install Homebrew
	/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

Install Dependencies (using homebrew)

	brew install mpfr gmp

Build the program
	clang++ -std=c++17 -O2 -Wall calculate_pi.cpp chudnovsky.cpp -o calculate_pi -lmpfr -lgmp -lpthread

TODO Update the Makefile and or install.sh
Platform Support

This project and install script has been tested on:

    Debian-based Linux (Ubuntu 22.04)
    macOS Monterey on Intel (Late 2014 Mac Mini)

The install.sh script will attempt to detect your operating system and install required dependencies like GMP, MPFR, and lm-sensors (for power monitoring). On macOS, it will prompt to install Homebrew if it's missing.

macOS on Apple Silicon (M1/M2) is theoretically supported but has not yet been tested.
Other Linux distributions (e.g., Arch, Fedora, openSUSE) are not currently supported by this script. You may need to install dependencies manually.

Benchmarks
PC
    Debian:     Bookwork V 12
    Processor:  Intel(R) Core(TM) i7-6700 CPU @ 3.40GHz 
    RAM:        32 GB
    
    Formula                             Decimal Places          Completion Time
    =============================================================================
    Gauss Legendre                         10,000,000                   30.021s
    Gauss Legendre                      1,000,000,000            1h 26m 49.000s
    Chudnovsky Single Thread                  100,000                   36.012s
    Chudnovsky Multi Thread (CPU 7)           300,000                2m 12.015s
    Chudnovsky Multi Thread (CPU 7)         1,000,000               33m  3.231s
    Chudnovsky Dynamic Thread (CPU 7)       1,000,000               26m 49.996s
    Chudnovsky Multi Thread (GPU ?)     &lt;TODO&gt;               &lt;TODO&gt;

MAC
    Mac OS Monterey V 12.
    Processor:  Intel i5 CPU @ 2.6 GHz
    RAM:        8 GB 
    Formula                        Decimal Places          Completion Time
    =========================================================================
    Gauss Legendre                  &lt;TODO&gt;                  &lt;TODO&gt;
    Chudnovsky Single Thread        &lt;TODO&gt;                  &lt;TODO&gt;
    Chudnovsky Multi Thread (CPU )  &lt;TODO&gt;                  &lt;TODO&gt;
