#include <iostream>
#include <fstream>
#include <string>
#include <mpfr.h>
#include <thread>
#include <atomic>
#include <locale>   // Needed for number formatting
#include <sstream>  // Required for std::istringstream
#include <cstring>  // For strcmp
#include <cmath>
#include <sys/stat.h>
#include <vector>
#include <algorithm>  // for std::min
#include <chrono>
#include <filesystem>
#include <mutex>
#include <getopt.h> // Add this at the top
#include <csignal>

#include "globals.hpp"
#include "chudnovsky.hpp"

static_assert(true, "Header included");

using namespace std;
namespace fs = std::filesystem;

// Global Variables
std::string version = "1.0.0";
std::string calculation_method = "gauss_legendre";  // Default calculation method
std::atomic <bool> keep_monitoring(true);           // Shared flag to stop monitoring thread
std::atomic <long long> iteration_counter(0);       // Global counter
std::atomic <long long> iterations(0);              // Ensure updates are visible to monitoring
//std::string reference_filename = "Pi-Dec-Chudnovsky_01.txt"; // Default
std::string reference_filename = "pi_reference_1M.txt"; // Default
std::atomic <bool> stop_requested(false);
std::mutex console_mutex;                           // Used to limit console output to a single thread at a time.
bool use_dynamic;                                   // Flag to indicate if chudnovsky dynamic thread allocation is used.

// External variables defined in globals.hpp
int debug_level = 0;                                // Debug level passed in from command line
bool use_multithreading = false;
long long decimal_places = 1;
int thread_count = 0;                               // Default is 0 = auto-detect based on CPU cores
mpfr_prec_t working_prec = 0;

struct RaplDomain
{
    std::string name;
    std::string energy_path;
    long long accumulated_energy_uj = 0;
    double last_power_watts = 0.0;
};

// Declare static variables for power monitoring function
static auto last_power_check_time = std::chrono::steady_clock::now();
std::vector<RaplDomain> rapl_domains;

// Allow Cntrl C to stop each therad.
void signal_handler(int signal) {
    if (signal == SIGINT) {
        stop_requested.store(true);
        std::cerr << "\n[Interrupt] Ctrl+C received. Stopping calculation...\n";
    }
}


// Caculate the working precision for the chudnovsky algorithm to use.
mpfr_prec_t get_chudnovsky_precision(int decimal_places, int buffer = 20000) 
{
    return static_cast<mpfr_prec_t>(ceil(decimal_places * 3.322 + buffer));
}


// Function to read Pi reference digits from file
std::string read_pi_from_file(const std::string& filename, size_t digits)
{
    std::ifstream file(filename.c_str());
    if (!file)
    {
        std::cerr << "Error: Cannot open file " << filename << std::endl;
        return "";
    }

    std::string pi_digits;
    pi_digits.reserve(digits + 2);  // "3." + digits

    char ch;
    while (file.get(ch) && pi_digits.length() < digits + 2)
    {
        if (std::isdigit(ch) || ch == '.')
        {
            pi_digits += ch;
        }
    }

    return pi_digits;
}

long get_free_memory_kb()
{
    std::ifstream meminfo("/proc/meminfo");
    std::string line;
    while (std::getline(meminfo, line))
    {
        if (line.find("MemAvailable:") == 0)   // Looks for available memory
        {
            std::istringstream iss(line);
            std::string key;
            long mem_kb;
            iss >> key >> mem_kb;
            return mem_kb;  // Value is in KB
        }
    }
    return -1;  // Return -1 if not found
}

// Function to read chudnovsky term and sum reference values in from a file
std::vector<std::string> load_reference_values(const std::string& filename)
{
    std::vector<std::string> values;
    std::ifstream file(filename);
    std::string line;
    while (std::getline(file, line))
    {
        if (!line.empty())
            values.push_back(line);
    }
    return values;
}

// Read power usage from the energy monitoring file
long long read_energy_uj(const std::string& path) {
    std::ifstream file(path);

    if (!file.is_open()) {
        std::cerr << "Error: Unable to open energy file at " << path << "\n";
        std::cerr << "Hint: You may need to run the program with elevated permissions (e.g., using sudo)\n";
        return -1;
    }
    else if (debug_level >= 4)
    {
        std::cerr << "The power usage file is open: " << path << "\n";
    }

    long long energy;
    file >> energy;
    return energy;
}

// Discover available RAPL power domains
//std::vector<RaplDomain> discover_rapl_domains() {
//    std::vector<RaplDomain> domains;
//    const std::string base_path = "/sys/class/powercap/";
//    try
//    {
//        for (const auto& entry : fs::recursive_directory_iterator(base_path))
//        {
//            if (entry.is_regular_file() && entry.path().filename() == "energy_uj")
//            {
//                std::ifstream test(entry.path());
//                if (test.is_open()) {
//                    RaplDomain domain;
//                    domain.energy_path = entry.path();
//                    domains.push_back(domain);
//                    if (debug_level >= 3)
//                    {
//                        std::lock_guard<std::mutex> lock(console_mutex);
//                        std::cerr << "[discover_rapl_domains] Usable domain: " << domain.energy_path << "\n";
//                    }
//                }
//                else
//                {
//                    if (debug_level >= 2)
//                    {
//                        std::lock_guard<std::mutex> lock(console_mutex);
//                        std::cerr << "[discover_rapl_domains] Skipped unreadable domain: " << entry.path() << "\n";
//                    }
//                }
//            }
//        }
//    }
//    catch (const std::exception& e)
//    {
//        std::lock_guard<std::mutex> lock(console_mutex);
//        std::cerr << "[discover_rapl_domains] Filesystem error: " << e.what() << "\n";
//    }
//    if (domains.empty())
//    {
//        std::lock_guard<std::mutex> lock(console_mutex);
//        std::cerr << "Warning: No readable power domains found. Power monitoring disabled.\n";
//        std::cerr << "NOTE: Execute with sudo to allow read permission on monitoring domains.\n";
//    }
//
//    return domains;
//}

// Discover available RAPL power domains
std::vector<RaplDomain> discover_rapl_domains() {
    std::vector<RaplDomain> domains;
    const std::string base_path = "/sys/class/powercap/";
    std::error_code ec;  // To catch filesystem errors without throwing
    try
    {
        for (const auto& entry : fs::recursive_directory_iterator(base_path, fs::directory_options::skip_permission_denied, ec))
        {
            if (entry.is_regular_file(ec) && entry.path().filename() == "energy_uj")
            {
                std::ifstream test(entry.path());
                if (test.is_open()) {
                    RaplDomain domain;
                    domain.energy_path = entry.path();
                    domains.push_back(domain);
                    if (debug_level >= 3)
                    {
                        std::lock_guard<std::mutex> lock(console_mutex);
                        std::cerr << "[discover_rapl_domains] Usable domain: " << domain.energy_path << "\n";
                    }
                }
                else
                {
                    if (debug_level >= 2)
                    {
                        std::lock_guard<std::mutex> lock(console_mutex);
                        std::cerr << "[discover_rapl_domains] Skipped unreadable domain: " << entry.path() << "\n";
                    }
                }
            }
            else if (ec) {
                if (debug_level >= 2)
                {
                    std::lock_guard<std::mutex> lock(console_mutex);
                    std::cerr << "[discover_rapl_domains] Skipped unreadable or broken entry: " << entry.path() << " (Reason: " << ec.message() << ")\n";
                }
                ec.clear();  // Clear after handling
            }
        }
    }
    catch (const std::exception& e)
    {
        std::lock_guard<std::mutex> lock(console_mutex);
        std::cerr << "[discover_rapl_domains] Filesystem exception: " << e.what() << "\n";
    }

    if (domains.empty())
    {
        std::lock_guard<std::mutex> lock(console_mutex);
        std::cerr << "⚠️ Warning: No readable power domains found. Power monitoring disabled.\n";
        std::cerr << "ℹ️  HINT: Try running with sudo if needed.\n";
    }

    return domains;
}


std::string format_domain_label(const std::string& name)
{
    if (name == "core") return   "CPU cores";
    if (name == "uncore") return "CPU other";
    if (name == "dram") return   "RAM";
    // Skip "package-0" by returning an empty string
    if (name == "package-0") return "";
    return name;
}

void print_power_usage()
{
    std::lock_guard<std::mutex> lock(console_mutex);    // Lock console output.

   if (rapl_domains.empty()) {
        if (debug_level >= 2) {
            std::cerr << "[Info] No energy data to report (power monitoring disabled).\n";
        }
        return; // Nothing to print
    }

    const int label_width = 15; // Total width to align labels

    double total_power = 0;
    std::cerr << "\n--- Power Usage by Domain ---\n";

    for (const auto& domain : rapl_domains)
    {
        std::string label = format_domain_label(domain.name);
        if (label.empty()) continue; // Skip "package-0"

        int padding = label_width - static_cast<int>(label.length());
        padding = std::max(0, padding); // Prevent negative spacing

        std::cerr << label << std::string(padding, ' ') << ": "
                  << std::fixed << std::setprecision(5)
                  << domain.last_power_watts << " W\n";

        total_power += domain.last_power_watts;
    }

    std::string total_label = "Total";
    int total_padding = label_width - static_cast<int>(total_label.length());
    total_padding = std::max(0, total_padding);

    std::cerr << total_label << std::string(total_padding, ' ') << ": "
              << std::fixed << std::setprecision(5)
              << total_power << " W\n";
}

void monitor_power_usage()
{
    static bool domains_initialized = false;
    static bool power_disabled      = false;

    if (!domains_initialized) {
        rapl_domains = discover_rapl_domains();
        domains_initialized = true;

        if (rapl_domains.empty()) {
            power_disabled = true;
        }
    }

    // Skip power monitoring if disabled
    if (power_disabled) {
        return;
    }

    // Proceed with power reading
    std::vector<long long> prev_energies(rapl_domains.size());
    std::vector<long long> curr_energies(rapl_domains.size());

    for (size_t i = 0; i < rapl_domains.size(); ++i) {
        prev_energies[i] = read_energy_uj(rapl_domains[i].energy_path);
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    for (size_t i = 0; i < rapl_domains.size(); ++i) {
        curr_energies[i] = read_energy_uj(rapl_domains[i].energy_path);
        long long delta_uj = curr_energies[i] - prev_energies[i];
        if (delta_uj < 0) delta_uj += (1LL << 32); // wraparound
        rapl_domains[i].accumulated_energy_uj += delta_uj;
        rapl_domains[i].last_power_watts = delta_uj / 1'000'000.0;
    }
}


void print_accumulated_energy()
{
    if (rapl_domains.empty()) {
        if (debug_level >= 2) {
            std::cerr << "[Info] No energy data to report (power monitoring disabled).\n";
        }
        return; // Nothing to print
    }
 
    const int energy_precision = 12;
    const int label_width = 15; // Total width to right-align labels before ':'

    double total_energy_wh = 0.0;
    std::cerr << "\n--- Accumulated Energy ---\n";

    for (const auto& domain : rapl_domains)
    {
        std::string label = format_domain_label(domain.name);
        if (label.empty()) continue; // Skip "package-0"

        double accumulated_energy_wh = domain.accumulated_energy_uj / 3'600'000'000.0;
        int padding = label_width - static_cast<int>(label.length());
        padding = std::max(0, padding); // Avoid negative padding

        std::cerr << label << std::string(padding, ' ') << ": "
                  << std::fixed << std::setprecision(energy_precision)
                  << accumulated_energy_wh << " Wh\n";

        total_energy_wh += accumulated_energy_wh;
    }

    std::string total_label = "Total";
    int total_padding = label_width - static_cast<int>(total_label.length());
    total_padding = std::max(0, total_padding);

    std::cerr << total_label << std::string(total_padding, ' ') << ": "
              << std::fixed << std::setprecision(energy_precision)
              << total_energy_wh << " Wh\n";
}

// Multithreaded Chudnovsky calculation
// Partition terms among threads
// Launch threads and wait for them to finish
// Combine partial results into final sum

void calculate_pi_multithreaded(
    int max_terms,
    mpfr_t pi_approx,
    const std::vector<std::string>& reference_terms,
    const std::vector<std::string>& reference_sums)
{
    if (debug_level >=2)
    {
       std::cerr << "Using " << thread_count << " worker threads for Chudnovsky calculation.\n";
    }

    iterations.store(max_terms, std::memory_order_relaxed);
    iteration_counter.store(0, std::memory_order_relaxed);

    // Precision estimate
    //  mpfr_prec_t working_prec = get_chudnovsky_precision(decimal_places);
    working_prec = get_chudnovsky_precision(decimal_places);
    if (debug_level >=2)
    {
        std::cout << "working precission= " << working_prec << " \n";
    }

    // Allocate space for thread results
    mpfr_t* shared_results = new mpfr_t[thread_count];
    for (int i = 0; i < thread_count; ++i)
    {
        mpfr_init2(shared_results[i], working_prec);
        mpfr_set_zero(shared_results[i], 1); // Positive zero
    }

    // Divide work
    int terms_per_thread = max_terms / thread_count;
    int leftover_terms = max_terms % thread_count;

    std::vector<std::thread> threads;
    int current_term = 0;

    if(debug_level >= 2)
    {
        std::cout << "max_terms = " << max_terms << "\n";
    }

    for (int i = 0; i < thread_count; ++i)
    {
        int extra = (i < leftover_terms) ? 1 : 0;
        int start = current_term;
        int end = start + terms_per_thread + extra;

        if (debug_level >= 2)
        {
            std::lock_guard<std::mutex> lock(console_mutex);
            std::cout << "[Thread " << i << "] k from " << start << " to " << (end - 1) << "\n";
        }

        threads.emplace_back(ChudnovskyTermCalculator::chudnovsky_worker, i, start, end, shared_results, working_prec, debug_level, reference_terms, reference_sums);
        current_term = end;
    }

    for (auto& t : threads)
    {
        t.join();
    }

    if (stop_requested.load())
    {
        std::cerr << "Calculation aborted by user.\n";
        delete[] shared_results;
        return; // Skip output and cleanup safely
    }

    // Display the shared_results
    if (debug_level >=3 )
    {
        for (int i = 0; i < thread_count; ++i)
        {
            std::cerr << "[Main] shared_results[" << i << "] = ";
            mpfr_out_str(stderr, 10, 100, shared_results[i], MPFR_RNDN);
            std::cerr << "\n";
        }
    }

//  Sum results
    mpfr_t total_sum;
    mpfr_init2(total_sum, working_prec);
    mpfr_set_zero(total_sum, 1);

    for (int i = 0; i < thread_count; ++i)
    {
        if (debug_level >= 3)
        {
            std::cerr << "[Main] shared_results[" << i << "] = ";
            mpfr_out_str(stderr, 10, 80, shared_results[i], MPFR_RNDN);
            std::cerr << "\n";
        }

        mpfr_add(total_sum, total_sum, shared_results[i], MPFR_RNDN);

        if (debug_level >= 3)
        {
            std::cerr << "Accumulated total after thread[" << i << "] = ";
            mpfr_out_str(stderr, 10, 80, total_sum, MPFR_RNDN);
            std::cerr << "\n";
        }

        mpfr_clear(shared_results[i]);
    }
    delete[] shared_results;

    // === CHUDNOVSKY CONSTANT C = 426880 * sqrt(10005) ===
    mpfr_t C, sqrt_10005;
    mpfr_init2(C, working_prec);
    mpfr_init2(sqrt_10005, working_prec);

    mpfr_sqrt_ui(sqrt_10005, 10005, MPFR_RNDN);
    mpfr_mul_ui(C, sqrt_10005, 426880, MPFR_RNDN);

    // === Final division: pi = C / sum ===
    mpfr_div(pi_approx, C, total_sum, MPFR_RNDN);

    mpfr_clear(total_sum);
    mpfr_clear(C);
    mpfr_clear(sqrt_10005);

    // Optional verbose final output
    if (debug_level >= 3)
    {
        mpfr_printf("Final computed pi = %.*Rf\n", decimal_places, pi_approx);
    }
    else
    {
        mpfr_printf("Final computed pi output to file name computed_pi.txt \n");
    }
}

// Function to monitor system in a separate thread
void monitor_system()
{
    static auto last_time = std::chrono::steady_clock::now();

    while (keep_monitoring)
    {
        // Sleep first so early values don’t confuse deltas
        for (int i = 0; i < 10 && keep_monitoring; ++i)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        auto current_time = std::chrono::steady_clock::now();
        last_time = current_time;

        // Show power usage for each RAPL domain
        if (debug_level >= 2)
        {
           // When I updated my HDD to SSD the power monitor stuff stopped working.
           // ChatGPT and I tried various different things but the rabbit hole kept getting deeper.
           // To continue uncomment the follwoing two lines and start debugging :) (--debug 2 and above)
           //monitor_power_usage(); //<TODO>
           //print_power_usage();  // <TODO>
        }

        // Show CPU Core Temperatures
        if (debug_level >= 2)
        {
            std::cout << "--- Checking CPU temperature ---\n";
            std::system("sensors | grep 'Core' | awk '{print $1, $2, $3}'");
        }

        // Show Available Free Memory
        if (debug_level >= 1)
        {
            long free_mem_kb = get_free_memory_kb();
            if (free_mem_kb != -1)
            {
                std::cout << "--- Available Memory ---\n " << free_mem_kb / 1024 << " MB\n";
            }
            else
            {
                std::cout << "--- Memory info unavailable ---\n";
            }
        }
        // Show Calculation Progress
        if (iterations != 0)
        {
            double progress = static_cast<double>(iteration_counter) / static_cast<double>(iterations) * 100;
            std::cout << "--- Calculation Progress ---\n" << iteration_counter << "/" << iterations
                      << " (" << progress << "%)\n";
        }
        else
        {
            std::cout << "--- Progress Initialization not complete yet (0%) ---\n";
        }
    }
//    <TODO> Power monitoring disabled due to glitches with detecting power domains automatically
//    if (debug_level >=2)
//    {
//        print_accumulated_energy();
//    }
}

// Function to output verification result
void print_verification_result(bool success, const std::string& method)
{
    if (success)
    {
        std::cout << method << ": \033[1;32mSUCCESS\033[0m" << std::endl;
    }
    else
    {
        std::cout << method << ": \033[1;31mFAILED\033[0m" << std::endl;
    }
}

void print_verification_result_unknown(off_t requested, off_t available)
{
    std::cout << "\033[1;34mPi verification: UNKNOWN (Requested "
              << requested << " places exceeds reference file length of "
              << available << ")\033[0m" << std::endl;
}

bool parse_command_line(int argc, char* argv[])
{
    bool decimal_places_set = false;
    int user_thread_request = -1;


    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];

        // Help option
        if (arg == "-h" || arg == "--help")
        {
            std::cerr <<"calculate pi Version " << version <<" By D Pollard and ChatGPT 2025 \n"
                      << "Usage: " << argv[0] << " <decimal_places> [options]\n\n"
                      << "Options:\n"
                      << "  -f, --file <filename>        Specify reference Pi file (default ./Pi-Dec-Chudnovsky_01.txt) \n"
                      << "  -d, --debug <1|2|3>          Set debug level (default: 0)\n"
                      << "  -m, --method <name>          Choose method: 'gauss_legendre' (default), 'chudnovsky'\n"
                      << "      --threads <count>        Number of threads to use (valid only for Chudnovsky) default is max -1\n"
                      << "  -h, --help                   Show this help message\n"
                      << "      --dynamic                Use dynamic work allocation with Chudnovsky multi threaded\n";
            return false; // Return false to prevent program from continuing
        }

        // Reference file
        else if (arg == "-f" || arg == "--file")
        {
            if (i + 1 < argc)
            {
                reference_filename = argv[++i];
            }
            else
            {
                std::cerr << "Error: " << arg << " requires a filename.\n";
                return false;
            }
        }

        // Debug level
        else if (arg == "-d" || arg == "--debug")
        {
            if (i + 1 < argc)
            {
                std::string level = argv[++i];
                if (level == "1") debug_level = 1;
                else if (level == "2") debug_level = 2;
                else if (level == "3") debug_level = 3;
                else
                {
                    std::cerr << "Error: " << arg << " must be 1, 2, or 3.\n";
                    return false;
                }
            }
            else
            {
                std::cerr << "Error: " << arg << " requires a level.\n";
                return false;
            }
        }

        // Method
        else if (arg == "-m" || arg == "--method")
        {
            if (i + 1 < argc)
            {
                calculation_method = argv[++i];
                if (calculation_method != "gauss_legendre" && calculation_method != "chudnovsky")
                {
                    std::cerr << "Error: Unknown method: " << calculation_method << "\n";
                    return false;
                }
            }
            else
            {
                std::cerr << "Error: " << arg << " requires a method name.\n";
                return false;
            }
        }

        // Threads
        else if (arg == "--threads")
        {
            if (i + 1 < argc)
            {
                try
                {
                    user_thread_request = std::stoi(argv[++i]);
                    if (user_thread_request <= 0)
                        throw std::invalid_argument("Thread count must be positive.");
                }
                catch (const std::exception& e)
                {
                    std::cerr << "Invalid thread count: " << e.what() << "\n";
                    return false;
                }
            }
            else
            {
                std::cerr << "Error: --threads requires a number.\n";
                return false;
            }
        }

        else if (arg == "--dynamic") 
        {
            use_dynamic = true;
        }

        // Unrecognized switch
        else if (arg[0] == '-')
        {
            std::cerr << "Error: Unknown option: " << arg << "\n";
            return false;
        }

        // Decimal places
        else if (!decimal_places_set)
        {
            try
            {
                decimal_places = std::stoll(arg);
                if (decimal_places <= 0)
                    throw std::invalid_argument("Decimal places must be positive.");
                decimal_places_set = true;
            }
            catch (const std::exception& e)
            {
                std::cerr << "Invalid decimal places: " << e.what() << "\n";
                return false;
            }
        }
        else
        {
            std::cerr << "Error: Multiple decimal place arguments provided.\n";
            return false;
        }
    }

    if (!decimal_places_set)
    {
        std::cerr << "Error: Decimal places argument is required.\n";
        return false;
    }

    // Set default method if not provided
    if (calculation_method.empty())
    {
        calculation_method = "gauss_legendre";
    }

    if (calculation_method == "chudnovsky")
    {
        int max_threads = std::max(1u, std::thread::hardware_concurrency() - 1);

        if (user_thread_request != -1)
        {
            if (user_thread_request > max_threads)
            {
                std::cerr << "Error: Requested thread count (" << user_thread_request
                          << ") exceeds maximum usable threads (" << max_threads << ").\n";
                return false;
            }
            thread_count = user_thread_request;
        }
        else
        {
            // No --threads given, use default
            thread_count = max_threads;
            std::cerr << "[Info] Using " << thread_count << " threads by default (available CPUs - 1)\n";
        }
    }
    else
    {
        std::cerr << "[Error] Unsupported method: " << calculation_method << "\n";
        return 1;
    }
    return true;
}


// Function to return the actual size of the file including any CR LF
off_t get_actual_file_size(const std::string& filename) {
    struct stat st;
    if (stat(filename.c_str(), &st) != 0) {
        // Error occurred
        return -1;
    }

    off_t actual_size = st.st_size;

    std::ifstream file(filename, std::ios::binary);
    if (file && actual_size >= 1) {
        // Check last character
        file.seekg(-1, std::ios::end);
        char last_char;
        file.get(last_char);
        if (last_char == '\n') {
            actual_size -= 1;

            // Check second last character (for CRLF case)
            if (actual_size >= 1) {
                file.seekg(-2, std::ios::end);
                char second_last_char;
                file.get(second_last_char);
                if (second_last_char == '\r') {
                    actual_size -= 1;
                }
            }
        } else if (last_char == '\r') {
            actual_size -= 1;
        }
    }
    return actual_size;
}

std::size_t find_divergence_location(const std::string& ref, const std::string& comp, std::size_t context = 10)
{
    std::size_t len = std::min(ref.size(), comp.size());
    for (std::size_t i = 0; i < len; ++i)
    {
        if (ref[i] != comp[i])
        {
            std::cout << "First mismatch at decimal place: " << i - 2
                      << " (char '" << ref[i] << "' vs '" << comp[i] << "')\n";

            // Show surrounding digits (up to 'context' digits before and after)
            std::size_t start = (i >= context) ? i - context : 0;
            std::size_t end = std::min(i + context + 1, len); // +1 to include mismatch

            std::string ref_context = ref.substr(start, end - start);
            std::string comp_context = comp.substr(start, end - start);

            std::cout << "Reference: " << ref_context << "\n";
            std::cout << "Computed : " << comp_context << "\n";

            return i; // Position where they first differ
        }
    }

    // If one string is longer, report that as divergence
    if (ref.size() != comp.size())
    {
        std::cout << "Strings differ in length at position: " << len << "\n";
        return len;
    }

    return std::string::npos; // No divergence
}

// Function to compare computed π with reference π from a file
void verify_pi_from_file(const std::string& computed_pi_str)
{
    //Check reference file size BEFORE opening and reading
    off_t file_size = get_actual_file_size(reference_filename);

    if (file_size == -1)
    {
        std::cerr << "Error: Unable to get size of reference file: " << reference_filename << std::endl;
        print_verification_result_unknown(decimal_places, 0);
        return;
    }

    // Calculate available decimal places
    off_t available_decimal_places = file_size - 2; // Minus 2 for "3."

    if (decimal_places > available_decimal_places)
    {
        print_verification_result_unknown(decimal_places, available_decimal_places);
        return; // Skip validation as file is too short
    }

    // Proceed as usual
    std::ifstream reference_file(reference_filename);
    if (!reference_file)
    {
        std::cerr << "Error: Could not open reference file." << std::endl;
        return;
    }

    // Read only the required number of characters from the reference file
    std::string reference_pi_str;
    reference_pi_str.reserve(decimal_places + 2); // Reserve space for "3." and the decimal places
    char ch;
    for (int i = 0; i < decimal_places + 2; ++i) // Read "3." and decimal_places digits
    {
        if (reference_file.get(ch))
        {
            reference_pi_str += ch;
        }
        else
        {
            std::cerr << "Error: Reference file is shorter than expected." << std::endl;
            return;
        }
    }

    // Truncate computed Pi to decimal_places + 2 (+2 is for "3.")
    std::string computed_pi_trimmed = computed_pi_str.substr(0, decimal_places + 2); // Keep "3." + requested digits

    // Truncate the reference Pi to the same length
    std::string reference_pi_trimmed = reference_pi_str.substr(0, decimal_places + 2);

    if (debug_level >= 2)
    {
        std::cout << "Reference Pi: " << reference_pi_trimmed << std::endl;
        std::cout << "Computed Pi:  " << computed_pi_trimmed << std::endl;
    }

    // Now compare the truncated values
    if (computed_pi_trimmed == reference_pi_trimmed)
    {
        print_verification_result(true, "Pi verification");
    }
    else
    {
        print_verification_result(false, "Pi verification");
    }
}

// Function to Output the computed value for pi to a file.
std::string write_computed_pi_to_file(const mpfr_t& pi_approx)
{
    char* computed_pi_cstr = nullptr;
    mpfr_asprintf(&computed_pi_cstr, "%.*Rf", static_cast<int>(decimal_places + 5), pi_approx);
    std::string computed_pi_str(computed_pi_cstr);
    mpfr_free_str(computed_pi_cstr);

    // Trim the computed_pi_str to requested decimal_places
    computed_pi_str = computed_pi_str.substr(0, decimal_places + 2); // "3." counts as first two characters

    std::ofstream pi_file("computed_pi.txt");
    if (pi_file)
    {
        pi_file << computed_pi_str << "\n";
        pi_file.close();
    }
    else
    {
        std::cerr << "Error: Could not open file for writing π value." << std::endl;
    }

    return computed_pi_str;  // RETURN here
}

// Function to calculate pi using the Gauss_Legendre algorithm.
void calculate_gauss_legendre_algorithm(mpfr_t pi_approx)
{
    iterations.store(log2(decimal_places) + 2, std::memory_order_relaxed);

    mpfr_t a, b, t, p, a_next, b_next, t_next, p_next, temp1, temp2;
    mpfr_inits(a, b, t, p, a_next, b_next, t_next, p_next, temp1, temp2, (mpfr_ptr) 0);

    // Initialize variables
    mpfr_set_d(a, 1.0, MPFR_RNDN);
    mpfr_sqrt_ui(b, 2, MPFR_RNDN);
    mpfr_ui_div(b, 1, b, MPFR_RNDN);
    mpfr_set_d(t, 0.25, MPFR_RNDN);
    mpfr_set_d(p, 1.0, MPFR_RNDN);

    for (long long i = 0; i < iterations; i++)
    {
        // std::cerr << "Loop iteration " << i << "\n"; // Output the loop number.

        iteration_counter.store(i + 1, std::memory_order_relaxed);  // For monitoring
        if (stop_requested.load(std::memory_order_seq_cst))
        {
            // std::cerr << "[Interrupt] Cntrl+C received. Cleaning up and exiting early.\n";
            mpfr_clears(a, b, t, p, a_next, b_next, t_next, p_next, temp1, temp2, pi_approx, (mpfr_ptr) 0);  // Clean up
            std::exit(1);  //exit cleanly without printing a result.
        }

        // a_next = (a + b) / 2.0
        mpfr_add(a_next, a, b, MPFR_RNDN);
        mpfr_div_ui(a_next, a_next, 2, MPFR_RNDN);

        // b_next = sqrt(a * b)
        mpfr_mul(b_next, a, b, MPFR_RNDN);
        mpfr_sqrt(b_next, b_next, MPFR_RNDN);

        // t_next = t - p * (a - a_next)^2
        mpfr_sub(temp1, a, a_next, MPFR_RNDN);
        mpfr_mul(temp1, temp1, temp1, MPFR_RNDN);
        mpfr_mul(temp1, p, temp1, MPFR_RNDN);
        mpfr_sub(t_next, t, temp1, MPFR_RNDN);

        // p_next = 2 * p
        mpfr_mul_ui(p_next, p, 2, MPFR_RNDN);

        // Update
        mpfr_set(a, a_next, MPFR_RNDN);
        mpfr_set(b, b_next, MPFR_RNDN);
        mpfr_set(t, t_next, MPFR_RNDN);
        mpfr_set(p, p_next, MPFR_RNDN);
    }

    // Compute pi_approx = (a + b)^2 / (4 * t)
    mpfr_add(temp1, a, b, MPFR_RNDN);
    mpfr_mul(temp1, temp1, temp1, MPFR_RNDN);
    mpfr_mul_ui(temp2, t, 4, MPFR_RNDN);
    mpfr_div(pi_approx, temp1, temp2, MPFR_RNDN);

    if (debug_level >=2)
    {
        mpfr_printf("Final computed pi_approx=%.*Rf\n", decimal_places, pi_approx);
    }

    // Free temp variables
    mpfr_clears(a, b, t, p, a_next, b_next, t_next, p_next, temp1, temp2, (mpfr_ptr) 0);
}

// Function to compute factorial of k using MPFR
void mpfr_factorial(mpfr_t result, unsigned long k)
{
    mpz_t fac;
    mpz_init(fac);
    mpz_fac_ui(fac, k);
    //mpfr_set_z(result, fac, MPFR_RNDN);
    mpfr_set_z(result, fac, MPFR_RNDZ);  // RNDZ = round toward zero (truncate)
    mpz_clear(fac);
}

void calculate_chudnovsky_algorithm(
    mpfr_t pi_approx,
    const std::vector<std::string>& reference_terms,
    const std::vector<std::string>& reference_sums)
{

    // Compute working precision in bits
    working_prec = get_chudnovsky_precision(decimal_places);
    if (debug_level >= 2)
    {
        printf("Working precision: %ld bits\n", working_prec);
    }

    // DJP Each method now sets it's own default precision
    // Set global MPFR default precision as a safety net
    //mpfr_set_default_prec(working_prec);

    // Call the single-threaded calculation function
    ChudnovskyTermCalculator calculator(working_prec, debug_level);
    calculator.calculate_chudnovsky_singlethreaded(pi_approx, working_prec, reference_terms, reference_sums);

}

// ********************   MAIN *******************************
int main(int argc, char* argv[])
{
    if (!parse_command_line(argc, argv))
    {
        return 1;  // Exit if parsing failed
    }

    // Reference data used while debugging.
    std::vector<std::string> reference_terms = load_reference_values("reference_terms.txt");
    std::vector<std::string> reference_sums  = load_reference_values("reference_sums.txt");

    // Register Stop Handler
    std::signal(SIGINT, signal_handler);

    // Start monitoring thread
    std::thread monitor_thread(monitor_system);

    std::locale loc("");
    std::cout.imbue(loc);

    if (debug_level >= 1)
    {
        std::cout << "[Main] Calculating pi to " << decimal_places << " decimal places.\n";
    }

    mpfr_t pi_approx;

    // ******************* Start Gauss Legendre   *******************
    if (calculation_method == "gauss_legendre")
    {
        std::cerr << "[Main] Using Single Threaded Gauss Legendre Algorithm \n";
        //<TODO> Consider changing precision to gl_precision
        long long precision = static_cast<long long>(decimal_places + 5) * 4;
        mpfr_set_default_prec(precision);  // Set function precision
        mpfr_init2(pi_approx, precision);
        calculate_gauss_legendre_algorithm(pi_approx);
    }

    // ******************* Start Chudnovsky   *******************
    else if (calculation_method == "chudnovsky")
    {

        // Compute chudnovsky default working precision in bits
        working_prec = get_chudnovsky_precision(decimal_places);
        mpfr_set_default_prec(working_prec);
        mpfr_init2(pi_approx, working_prec);

        if (debug_level >= 2)
        {
            std::cout << "[Main] MPFR default chudnovsky precision set to " << working_prec << " bits.\n";
        }
        
        // Validate thread count request from command line.
        int max_threads = std::max(1u, std::thread::hardware_concurrency());

        if (thread_count <= 0 || thread_count > max_threads)
        {
            std::cerr << "Warning: Requested " << thread_count << " threads, but only "
                  << (max_threads + 1) << " available.\n";
            thread_count = std::max(1, max_threads);  // adjust
            std::cerr << "Using " << thread_count << " threads instead.\n";
        }

        if (thread_count > 1)
        {
            if (use_dynamic)
            {
                std::cout << "[Main] Using dynamic multithreaded Chudnovsky Algorithm\n";
                set_dynamic_chunks(chunk_size);
                
                //calculate_pi_chudnovsky_dynamic(thread_count, pi_approx);
                calculate_pi_chudnovsky_dynamic(thread_count, pi_approx, reference_terms, reference_sums);
            }
            else 
            {
                std::cerr << "[Main] Using static multithreaded Chudnovsky Algorithm \n";
                calculate_pi_multithreaded(ChudnovskyTermCalculator::estimate_required_k(decimal_places), pi_approx, reference_terms, reference_sums);
            }
        }
        else
        {
            std::cerr << "[Main] Using single-threaded ChudnovskyAlgorithm \n";
            calculate_chudnovsky_algorithm(pi_approx, reference_terms, reference_sums);
        }
    }
    
    // Output the computed value to file
    std::string computed_pi_str = write_computed_pi_to_file(pi_approx);

    // Trim to requested decimal places
    computed_pi_str = computed_pi_str.substr(0, decimal_places + 2); // + 2 for "3."

    // Verify result
    verify_pi_from_file(computed_pi_str);

    // Stop monitoring thread
    keep_monitoring = false;
    monitor_thread.join();

    // Free memory
    mpfr_clear(pi_approx);
    
    std::cout << "[Main] Done.\n";
    return 0;
}
