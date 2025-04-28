# Create reference values for calculate_pi.cpp
# Output is for k values from 0 to 6
# chudnovsky sum values
# chudnovsky term values
# These are used to compare against the values calculated in C++
# when in debug level 3 or higher.

from mpmath import mp, fac, power, ceil

# Set decimal places to match your C++ program
decimal_places = 1000  # Adjust to match your `decimal_places` in C++
buffer = 20000         # Match your buffer in bits

# Convert to equivalent mpmath precision in decimal digits
mp.dps = ceil((decimal_places * 3.322 + buffer) / 3.322) 

def chudnovsky_term(k):
    """Compute a single term of the Chudnovsky series."""
    numerator = (-1) ** k * fac(6 * k) * (545140134 * k + 13591409)
    denominator = fac(3 * k) * (fac(k) ** 3) * power(640320, 3 * k)
    return numerator / denominator

# Compute sum for various k values
max_k = 6  # Adjust as needed
sum_value = 0

print(f"Chudnovsky sum values (matching precision: {mp.dps} digits):\n")
for k in range(max_k + 1):
    term = chudnovsky_term(k)
    sum_value += term
    print(f"k={k}, sum={sum_value}")

