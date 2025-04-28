import mpmath

# Set precision high enough for 1000 digits
mpmath.mp.dps = 1050

# Chudnovsky formula constants
C = 426880 * mpmath.sqrt(10005)

def factorial(n):
    return mpmath.fac(n)

def chudnovsky_term(k):
    num = (-1)**k * factorial(6*k) * (13591409 + 545140134*k)
    denom = factorial(3*k) * (factorial(k)**3) * (640320**(3*k))
    return num / denom

# Compute and truncate the first 7 terms
terms = [chudnovsky_term(k) for k in range(7)]
truncated_terms = [str(term)[:1000] for term in terms]

for k, term in enumerate(truncated_terms):
    print(f'k={k}: {term}')
