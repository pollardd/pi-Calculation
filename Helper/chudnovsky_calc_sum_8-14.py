# Create reference values for calculate_pi.cpp
# Output is for k values from 8 to 14
# chudnovsky sum values
# These are used to compare against the values calculated in C++
# when in debug level 3 or higher.

from mpmath import mp, fac, power, ceil

decimal_places = 1000
buffer = 20000
mp.dps = ceil((decimal_places * 3.322 + buffer) / 3.322)

def chudnovsky_term(k):
    return (-1)**k * fac(6*k) * (545140134*k + 13591409) \
         / (fac(3*k) * fac(k)**3 * power(640320, 3*k))

start_k = 8
end_k   = 14  # inclusive
running = mp.mpf('0')
for k in range(start_k, end_k+1):
    running += chudnovsky_term(k)
    print(str(running))   # one line per k
