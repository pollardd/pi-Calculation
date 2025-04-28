#pragma once
#ifndef CHUDNOVSKY_HPP
#define CHUDNOVSKY_HPP

#include <mpfr.h>
#include <vector>
#include <string>

// Forward declaration
struct ChudnovskyScratchpad;

class ChudnovskyTermCalculator {
public:
    ChudnovskyTermCalculator(mpfr_prec_t precision, int debug_level);
    ~ChudnovskyTermCalculator();

    //void compute_term(mpfr_t result, unsigned long k);
    void compute_term(mpfr_t result, unsigned long k, ChudnovskyScratchpad& scratch);

    static void chudnovsky_worker(
        int thread_id,
        int start_term,
        int end_term,
        mpfr_t* shared_results,
        mpfr_prec_t prec,
        int debug_level,
        const std::vector<std::string>& reference_terms,
        const std::vector<std::string>& reference_sums);

    static unsigned long estimate_required_k(long long decimal_places);

    void calculate_chudnovsky_singlethreaded(mpfr_t pi_approx,
                                             mpfr_prec_t working_prec,
                                             const std::vector<std::string>& reference_terms,
                                             const std::vector<std::string>& reference_sums);

    void init_scratchpad_at_k(ChudnovskyScratchpad& scratch, unsigned long k);

    //void compare_value(const std::string& label, const mpfr_t value, const std::vector<std::string>& reference_values, int k, mpfr_prec_t working_prec);
    void compare_value(const std::string& label, const mpfr_t value, const std::vector<std::string>& reference_values, int k, long long decimal_places);

    void compare_strings_verbose(const std::string& label, const std::string& s1, const std::string& s2);

private:
    mpfr_prec_t prec;
    int debug;

    // Scratchpad variables
    mpfr_t mp_k, mp_3k, power_neg1;
    //mpfr_t fact_k, fact_3k, fact_6k, fact_k3;
    mpfr_t power_640320;
    mpfr_t multiplier, num, den;
};

#endif
