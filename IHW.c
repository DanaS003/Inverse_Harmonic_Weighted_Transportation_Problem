#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <math.h>

#ifdef _WIN32
  #include <io.h>
  #define isatty _isatty
  #define fileno _fileno
  #include <windows.h>
#else
  #include <unistd.h>
  #include <dirent.h>
  #include <strings.h>
#endif

/* =========================
 * Utilities
 * ========================= */

static void rstrip_newline(char *s) {
    if (!s) return;

    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) {
        s[n - 1] = '\0';
        n--;
    }
}

static int ends_with_txt(const char *name) {
    if (!name) return 0;

    size_t n = strlen(name);
    if (n < 4) return 0;

    const char *ext = name + (n - 4);

    return (tolower((unsigned char)ext[0]) == '.' &&
            tolower((unsigned char)ext[1]) == 't' &&
            tolower((unsigned char)ext[2]) == 'x' &&
            tolower((unsigned char)ext[3]) == 't');
}

static long long getSmallerLongLong(long long firstValue, long long secondValue) {
    return (firstValue < secondValue) ? firstValue : secondValue;
}

static double getSmallerDouble(double firstValue, double secondValue) {
    return (firstValue < secondValue) ? firstValue : secondValue;
}

static int isLessThanWithTolerance(double firstValue, double secondValue, double epsilon) {
    return firstValue < (secondValue - epsilon);
}

static int isEqualWithTolerance(double firstValue, double secondValue, double epsilon) {
    return fabs(firstValue - secondValue) <= epsilon;
}

static void join_path(char *out, size_t out_sz, const char *folder, const char *file) {
#ifdef _WIN32
    snprintf(out, out_sz, "%s\\%s", folder, file);
#else
    snprintf(out, out_sz, "%s/%s", folder, file);
#endif
}

static void copy_uppercase(char *out, size_t out_sz, const char *in) {
    if (out_sz == 0) return;

    size_t i = 0;
    for (; in[i] != '\0' && i + 1 < out_sz; i++) {
        out[i] = (char)toupper((unsigned char)in[i]);
    }
    out[i] = '\0';
}

static int digits_int(long long value) {
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%lld", value);
    return (int)strlen(buffer);
}

static int max_digits_long_long_array(const long long *values, int count) {
    int width = 1;

    for (int i = 0; i < count; i++) {
        int d = digits_int(values[i]);
        if (d > width) width = d;
    }

    return width;
}

static int max_digits_double_as_int_array(const double *values, int count) {
    int width = 1;

    for (int i = 0; i < count; i++) {
        long long v = (long long)llround(values[i]);
        int d = digits_int(v);
        if (d > width) width = d;
    }

    return width;
}

static void printDoublePythonLike(double value) {
    /*
     * Python repr(float) dan printf C bisa beda 1-2 digit terakhir.
     * Format ini dipakai agar nilai tetap detail seperti output Python.
     */
    printf("%.17g", value);
}

static void printIntArrayLikeNumpyFromLongLong(const long long *values, int count) {
    int width = max_digits_long_long_array(values, count);

    printf("[");
    for (int i = 0; i < count; i++) {
        if (i > 0) printf(" ");
        printf("%*lld", width, values[i]);
    }
    printf("]");
}

static void printIntArrayLikeNumpyFromDouble(const double *values, int count) {
    int width = max_digits_double_as_int_array(values, count);

    printf("[");
    for (int i = 0; i < count; i++) {
        if (i > 0) printf(" ");
        printf("%*lld", width, (long long)llround(values[i]));
    }
    printf("]");
}

static void printDoubleCostsVectorLikeNumpy(const double *values, int count) {
    printf("[");
    for (int i = 0; i < count; i++) {
        if (i > 0) printf(" ");
        printf("%.0f.", values[i]);
    }
    printf("]");
}

/* =========================
 * Problem data
 * ========================= */

typedef struct {
    int m, n;
    long long *cost;
    long long *supply;
    long long *demand;
    long long effective;
    int dummy_added;
    char dummy_type;
    int dummy_index;
} Problem;

static void freeProblem(Problem *problem) {
    if (!problem) return;

    free(problem->cost);
    free(problem->supply);
    free(problem->demand);

    problem->cost = NULL;
    problem->supply = NULL;
    problem->demand = NULL;
}

static inline long long COST(const Problem *problem, int sourceIndex, int destinationIndex) {
    return problem->cost[(size_t)sourceIndex * (size_t)problem->n + (size_t)destinationIndex];
}

static inline void SET_COST(Problem *problem, int sourceIndex, int destinationIndex, long long value) {
    problem->cost[(size_t)sourceIndex * (size_t)problem->n + (size_t)destinationIndex] = value;
}

/* =========================
 * Input data
 * ========================= */

static int readTransportationProblemFile(const char *filePath, Problem *problem) {
    memset(problem, 0, sizeof(*problem));

    FILE *file = fopen(filePath, "r");
    if (!file) return 0;

    int numberOfSources = 0;
    int numberOfDestinations = 0;

    if (fscanf(file, "%d %d", &numberOfSources, &numberOfDestinations) != 2 ||
        numberOfSources <= 0 ||
        numberOfDestinations <= 0) {
        fclose(file);
        return 0;
    }

    problem->m = numberOfSources;
    problem->n = numberOfDestinations;

    problem->cost = (long long*)calloc((size_t)numberOfSources * (size_t)numberOfDestinations, sizeof(long long));
    problem->supply = (long long*)calloc((size_t)numberOfSources, sizeof(long long));
    problem->demand = (long long*)calloc((size_t)numberOfDestinations, sizeof(long long));

    problem->effective = -1;
    problem->dummy_added = 0;
    problem->dummy_type = 0;
    problem->dummy_index = -1;

    if (!problem->cost || !problem->supply || !problem->demand) {
        fclose(file);
        freeProblem(problem);
        return 0;
    }

    for (int sourceIndex = 0; sourceIndex < numberOfSources; sourceIndex++) {
        for (int destinationIndex = 0; destinationIndex < numberOfDestinations; destinationIndex++) {
            long long costValue;

            if (fscanf(file, "%lld", &costValue) != 1) {
                fclose(file);
                freeProblem(problem);
                return 0;
            }

            SET_COST(problem, sourceIndex, destinationIndex, costValue);
        }
    }

    for (int sourceIndex = 0; sourceIndex < numberOfSources; sourceIndex++) {
        long long supplyValue;

        if (fscanf(file, "%lld", &supplyValue) != 1) {
            fclose(file);
            freeProblem(problem);
            return 0;
        }

        problem->supply[sourceIndex] = supplyValue;
    }

    for (int destinationIndex = 0; destinationIndex < numberOfDestinations; destinationIndex++) {
        long long demandValue;

        if (fscanf(file, "%lld", &demandValue) != 1) {
            fclose(file);
            freeProblem(problem);
            return 0;
        }

        problem->demand[destinationIndex] = demandValue;
    }

    long long effectiveCost;
    if (fscanf(file, "%lld", &effectiveCost) == 1) {
        problem->effective = effectiveCost;
    }

    fclose(file);
    return 1;
}

/* =========================
 * Balancing
 * ========================= */

static int balanceTransportationProblem(Problem *problem) {
    long long totalSupply = 0;
    long long totalDemand = 0;

    for (int sourceIndex = 0; sourceIndex < problem->m; sourceIndex++) {
        totalSupply += problem->supply[sourceIndex];
    }

    for (int destinationIndex = 0; destinationIndex < problem->n; destinationIndex++) {
        totalDemand += problem->demand[destinationIndex];
    }

    if (totalSupply == totalDemand) {
        problem->dummy_added = 0;
        return 1;
    }

    if (totalSupply > totalDemand) {
        long long difference = totalSupply - totalDemand;
        int newNumberOfDestinations = problem->n + 1;

        long long *newCost = (long long*)calloc((size_t)problem->m * (size_t)newNumberOfDestinations, sizeof(long long));
        long long *newDemand = (long long*)calloc((size_t)newNumberOfDestinations, sizeof(long long));

        if (!newCost || !newDemand) {
            free(newCost);
            free(newDemand);
            return 0;
        }

        for (int sourceIndex = 0; sourceIndex < problem->m; sourceIndex++) {
            for (int destinationIndex = 0; destinationIndex < problem->n; destinationIndex++) {
                newCost[(size_t)sourceIndex * (size_t)newNumberOfDestinations + (size_t)destinationIndex] =
                    COST(problem, sourceIndex, destinationIndex);
            }

            newCost[(size_t)sourceIndex * (size_t)newNumberOfDestinations + (size_t)(newNumberOfDestinations - 1)] = 0;
        }

        for (int destinationIndex = 0; destinationIndex < problem->n; destinationIndex++) {
            newDemand[destinationIndex] = problem->demand[destinationIndex];
        }

        newDemand[newNumberOfDestinations - 1] = difference;

        free(problem->cost);
        free(problem->demand);

        problem->cost = newCost;
        problem->demand = newDemand;
        problem->n = newNumberOfDestinations;

        problem->dummy_added = 1;
        problem->dummy_type = 'c';
        problem->dummy_index = newNumberOfDestinations - 1;

        return 1;
    } else {
        long long difference = totalDemand - totalSupply;
        int newNumberOfSources = problem->m + 1;

        long long *newCost = (long long*)calloc((size_t)newNumberOfSources * (size_t)problem->n, sizeof(long long));
        long long *newSupply = (long long*)calloc((size_t)newNumberOfSources, sizeof(long long));

        if (!newCost || !newSupply) {
            free(newCost);
            free(newSupply);
            return 0;
        }

        for (int sourceIndex = 0; sourceIndex < problem->m; sourceIndex++) {
            for (int destinationIndex = 0; destinationIndex < problem->n; destinationIndex++) {
                newCost[(size_t)sourceIndex * (size_t)problem->n + (size_t)destinationIndex] =
                    COST(problem, sourceIndex, destinationIndex);
            }
        }

        for (int sourceIndex = 0; sourceIndex < problem->m; sourceIndex++) {
            newSupply[sourceIndex] = problem->supply[sourceIndex];
        }

        newSupply[newNumberOfSources - 1] = difference;

        free(problem->cost);
        free(problem->supply);

        problem->cost = newCost;
        problem->supply = newSupply;
        problem->m = newNumberOfSources;

        problem->dummy_added = 1;
        problem->dummy_type = 'r';
        problem->dummy_index = newNumberOfSources - 1;

        return 1;
    }
}

/* =========================
 * IHW solver core
 * ========================= */

static double calculateRowIchmPenalty(const Problem *problem, int sourceIndex, const unsigned char *inactiveDestination) {
    double sumCost = 0.0;
    double sumSquaredCost = 0.0;

    for (int destinationIndex = 0; destinationIndex < problem->n; destinationIndex++) {
        if (inactiveDestination[destinationIndex]) continue;

        double costValue = (double)COST(problem, sourceIndex, destinationIndex);

        sumCost += costValue;
        sumSquaredCost += costValue * costValue;
    }

    if (sumCost == 0.0 || sumSquaredCost == 0.0) return 0.0;

    return sumCost / sumSquaredCost;
}

static double calculateColumnIchmPenalty(const Problem *problem, int destinationIndex, const unsigned char *inactiveSource) {
    double sumCost = 0.0;
    double sumSquaredCost = 0.0;

    for (int sourceIndex = 0; sourceIndex < problem->m; sourceIndex++) {
        if (inactiveSource[sourceIndex]) continue;

        double costValue = (double)COST(problem, sourceIndex, destinationIndex);

        sumCost += costValue;
        sumSquaredCost += costValue * costValue;
    }

    if (sumCost == 0.0 || sumSquaredCost == 0.0) return 0.0;

    return sumCost / sumSquaredCost;
}

static void findBestMinimumCostCellInRow(const Problem *problem,
                                         int sourceIndex,
                                         const unsigned char *inactiveDestination,
                                         const double *currentSupply,
                                         const double *currentDemand,
                                         double *outMinimumCost,
                                         int *outBestDestination,
                                         double *outBestFeasibleAllocation) {
    double minimumCost = INFINITY;

    for (int destinationIndex = 0; destinationIndex < problem->n; destinationIndex++) {
        if (inactiveDestination[destinationIndex]) continue;

        double costValue = (double)COST(problem, sourceIndex, destinationIndex);

        if (costValue < minimumCost) {
            minimumCost = costValue;
        }
    }

    int bestDestination = -1;
    double bestFeasibleAllocation = INFINITY;

    for (int destinationIndex = 0; destinationIndex < problem->n; destinationIndex++) {
        if (inactiveDestination[destinationIndex]) continue;

        double costValue = (double)COST(problem, sourceIndex, destinationIndex);

        if (!isEqualWithTolerance(costValue, minimumCost, 1e-12)) continue;

        double feasibleAllocation = getSmallerDouble(currentSupply[sourceIndex], currentDemand[destinationIndex]);

        if (feasibleAllocation < bestFeasibleAllocation) {
            bestFeasibleAllocation = feasibleAllocation;
            bestDestination = destinationIndex;
        }
    }

    if (bestDestination < 0) {
        bestDestination = -1;
        bestFeasibleAllocation = 0.0;
        minimumCost = 0.0;
    }

    *outMinimumCost = minimumCost;
    *outBestDestination = bestDestination;
    *outBestFeasibleAllocation = bestFeasibleAllocation;
}

static void findBestMinimumCostCellInColumn(const Problem *problem,
                                            int destinationIndex,
                                            const unsigned char *inactiveSource,
                                            const double *currentSupply,
                                            const double *currentDemand,
                                            double *outMinimumCost,
                                            int *outBestSource,
                                            double *outBestFeasibleAllocation) {
    double minimumCost = INFINITY;

    for (int sourceIndex = 0; sourceIndex < problem->m; sourceIndex++) {
        if (inactiveSource[sourceIndex]) continue;

        double costValue = (double)COST(problem, sourceIndex, destinationIndex);

        if (costValue < minimumCost) {
            minimumCost = costValue;
        }
    }

    int bestSource = -1;
    double bestFeasibleAllocation = INFINITY;

    for (int sourceIndex = 0; sourceIndex < problem->m; sourceIndex++) {
        if (inactiveSource[sourceIndex]) continue;

        double costValue = (double)COST(problem, sourceIndex, destinationIndex);

        if (!isEqualWithTolerance(costValue, minimumCost, 1e-12)) continue;

        double feasibleAllocation = getSmallerDouble(currentSupply[sourceIndex], currentDemand[destinationIndex]);

        if (feasibleAllocation < bestFeasibleAllocation) {
            bestFeasibleAllocation = feasibleAllocation;
            bestSource = sourceIndex;
        }
    }

    if (bestSource < 0) {
        bestSource = -1;
        bestFeasibleAllocation = 0.0;
        minimumCost = 0.0;
    }

    *outMinimumCost = minimumCost;
    *outBestSource = bestSource;
    *outBestFeasibleAllocation = bestFeasibleAllocation;
}

typedef struct {
    int is_row;
    int idx;
    double weight;
    double min_cost;
    double tie_qty;
    int best_partner;
} Candidate;

static Candidate createRowCandidate(int rowIndex,
                                    double rowWeight,
                                    double rowMinimumCost,
                                    double rowFeasibleAllocation,
                                    int rowBestDestination) {
    Candidate candidate;

    candidate.is_row = 1;
    candidate.idx = rowIndex;
    candidate.weight = rowWeight;
    candidate.min_cost = rowMinimumCost;
    candidate.tie_qty = rowFeasibleAllocation;
    candidate.best_partner = rowBestDestination;

    return candidate;
}

static Candidate createColumnCandidate(int destinationIndex,
                                       double columnWeight,
                                       double columnMinimumCost,
                                       double columnFeasibleAllocation,
                                       int columnBestSource) {
    Candidate candidate;

    candidate.is_row = 0;
    candidate.idx = destinationIndex;
    candidate.weight = columnWeight;
    candidate.min_cost = columnMinimumCost;
    candidate.tie_qty = columnFeasibleAllocation;
    candidate.best_partner = columnBestSource;

    return candidate;
}

static int getCandidateBestAllocationQuantityForTie(const Problem *problem,
                                                     const Candidate *candidate,
                                                     const unsigned char *inactiveSource,
                                                     const unsigned char *inactiveDestination,
                                                     const double *currentSupply,
                                                     const double *currentDemand,
                                                     double *outBestQuantity) {
    double bestQty = INFINITY;

    if (candidate->is_row) {
        int rowIndex = candidate->idx;
        double minCost = INFINITY;

        for (int destinationIndex = 0; destinationIndex < problem->n; destinationIndex++) {
            if (inactiveDestination[destinationIndex]) continue;
            double costValue = (double)COST(problem, rowIndex, destinationIndex);
            if (costValue < minCost) minCost = costValue;
        }

        if (!isfinite(minCost)) return 0;

        for (int destinationIndex = 0; destinationIndex < problem->n; destinationIndex++) {
            if (inactiveDestination[destinationIndex]) continue;
            double costValue = (double)COST(problem, rowIndex, destinationIndex);
            if (!isEqualWithTolerance(costValue, minCost, 1e-12)) continue;

            double qty = getSmallerDouble(currentSupply[rowIndex], currentDemand[destinationIndex]);
            if (qty < bestQty) bestQty = qty;
        }
    } else {
        int destinationIndex = candidate->idx;
        double minCost = INFINITY;

        for (int sourceIndex = 0; sourceIndex < problem->m; sourceIndex++) {
            if (inactiveSource[sourceIndex]) continue;
            double costValue = (double)COST(problem, sourceIndex, destinationIndex);
            if (costValue < minCost) minCost = costValue;
        }

        if (!isfinite(minCost)) return 0;

        for (int sourceIndex = 0; sourceIndex < problem->m; sourceIndex++) {
            if (inactiveSource[sourceIndex]) continue;
            double costValue = (double)COST(problem, sourceIndex, destinationIndex);
            if (!isEqualWithTolerance(costValue, minCost, 1e-12)) continue;

            double qty = getSmallerDouble(currentSupply[sourceIndex], currentDemand[destinationIndex]);
            if (qty < bestQty) bestQty = qty;
        }
    }

    if (!isfinite(bestQty)) return 0;

    *outBestQuantity = bestQty;
    return 1;
}

static Candidate chooseBestCandidate(const Problem *problem,
                                     const Candidate *rowCandidates,
                                     int rowCandidateCount,
                                     const Candidate *columnCandidates,
                                     int columnCandidateCount,
                                     const unsigned char *inactiveSource,
                                     const unsigned char *inactiveDestination,
                                     const double *currentSupply,
                                     const double *currentDemand,
                                     double epsilon,
                                     int verbose) {
    Candidate allCandidates[2048];
    int allCandidateCount = 0;

    for (int i = 0; i < rowCandidateCount; i++) {
        allCandidates[allCandidateCount++] = rowCandidates[i];
    }

    for (int i = 0; i < columnCandidateCount; i++) {
        allCandidates[allCandidateCount++] = columnCandidates[i];
    }

    Candidate emptyCandidate;
    emptyCandidate.is_row = 1;
    emptyCandidate.idx = -1;
    emptyCandidate.weight = INFINITY;
    emptyCandidate.min_cost = INFINITY;
    emptyCandidate.tie_qty = INFINITY;
    emptyCandidate.best_partner = -1;

    if (allCandidateCount == 0) return emptyCandidate;

    double priorityValue = allCandidates[0].weight;
    for (int i = 1; i < allCandidateCount; i++) {
        if (allCandidates[i].weight < priorityValue) {
            priorityValue = allCandidates[i].weight;
        }
    }

    Candidate tiedOptions[2048];
    int tiedCount = 0;

    for (int i = 0; i < allCandidateCount; i++) {
        if (isEqualWithTolerance(allCandidates[i].weight, priorityValue, epsilon)) {
            tiedOptions[tiedCount++] = allCandidates[i];
        }
    }

    if (tiedCount == 1) {
        if (verbose) {
            printf("\n-> Prioritas dipilih: %s %d.\n",
                   tiedOptions[0].is_row ? "Row" : "Col",
                   tiedOptions[0].idx + 1);
        }
        return tiedOptions[0];
    }

    if (verbose) {
        printf("\n-> ADA NILAI SAMA PADA PENALTI 2. Solve:\n");
    }

    double minCostForTiedOptions = INFINITY;
    Candidate tiedByCost[2048];
    int tiedByCostCount = 0;

    for (int i = 0; i < tiedCount; i++) {
        double minCostCurrent = tiedOptions[i].min_cost;

        if (minCostCurrent < minCostForTiedOptions - epsilon) {
            minCostForTiedOptions = minCostCurrent;
            tiedByCostCount = 0;
            tiedByCost[tiedByCostCount++] = tiedOptions[i];
        } else if (isEqualWithTolerance(minCostCurrent, minCostForTiedOptions, epsilon)) {
            tiedByCost[tiedByCostCount++] = tiedOptions[i];
        }
    }

    if (tiedByCostCount == 1) {
        if (verbose) {
            printf("   - Seri dipecahkan berdasarkan biaya matriks awal terkecil. Terpilih: %s %d.\n",
                   tiedByCost[0].is_row ? "Row" : "Col",
                   tiedByCost[0].idx + 1);
        }
        return tiedByCost[0];
    }

    if (verbose) {
        printf("   - Masih sama pada biaya matriks. Solve: berdasarkan alokasi TERKECIL.\n");
    }

    Candidate bestCandidate = tiedByCost[0];
    double bestQty = INFINITY;

    for (int i = 0; i < tiedByCostCount; i++) {
        double qty = INFINITY;

        if (!getCandidateBestAllocationQuantityForTie(problem,
                                                       &tiedByCost[i],
                                                       inactiveSource,
                                                       inactiveDestination,
                                                       currentSupply,
                                                       currentDemand,
                                                       &qty)) {
            continue;
        }

        if (qty < bestQty) {
            bestQty = qty;
            bestCandidate = tiedByCost[i];
        }
    }

    if (verbose) {
        printf("   - Solve: Terpilih: %s %d dengan alokasi sebesar %lld.\n",
               bestCandidate.is_row ? "Row" : "Col",
               bestCandidate.idx + 1,
               (long long)llround(bestQty));
    }

    return bestCandidate;
}

static void getActiveRowCosts(const Problem *problem,
                              int sourceIndex,
                              const unsigned char *inactiveDestination,
                              double *outValues,
                              int *outCount) {
    int count = 0;

    for (int destinationIndex = 0; destinationIndex < problem->n; destinationIndex++) {
        if (inactiveDestination[destinationIndex]) continue;
        outValues[count++] = (double)COST(problem, sourceIndex, destinationIndex);
    }

    *outCount = count;
}

static void getActiveColumnCosts(const Problem *problem,
                                 int destinationIndex,
                                 const unsigned char *inactiveSource,
                                 double *outValues,
                                 int *outCount) {
    int count = 0;

    for (int sourceIndex = 0; sourceIndex < problem->m; sourceIndex++) {
        if (inactiveSource[sourceIndex]) continue;
        outValues[count++] = (double)COST(problem, sourceIndex, destinationIndex);
    }

    *outCount = count;
}

static int allDoubleArrayZeroWithTolerance(const double *values, int count, double epsilon) {
    for (int i = 0; i < count; i++) {
        if (fabs(values[i]) > epsilon) return 0;
    }
    return 1;
}

static void printProblemCostMatrixLongLongLikeNumpy(const Problem *problem) {
    printf("[");
    for (int sourceIndex = 0; sourceIndex < problem->m; sourceIndex++) {
        if (sourceIndex > 0) printf(" ");
        printf("[");
        for (int destinationIndex = 0; destinationIndex < problem->n; destinationIndex++) {
            if (destinationIndex > 0) printf(" ");
            printf("%lld", COST(problem, sourceIndex, destinationIndex));
        }
        printf("]");
        if (sourceIndex < problem->m - 1) printf("\n");
    }
    printf("]\n");
}

static void printProblemCostMatrixDoubleLikeNumpy(const Problem *problem) {
    printf("[");
    for (int sourceIndex = 0; sourceIndex < problem->m; sourceIndex++) {
        if (sourceIndex > 0) printf(" ");
        printf("[");
        for (int destinationIndex = 0; destinationIndex < problem->n; destinationIndex++) {
            if (destinationIndex > 0) printf(" ");
            printf("%.0f.", (double)COST(problem, sourceIndex, destinationIndex));
        }
        printf("]");
        if (sourceIndex < problem->m - 1) printf("\n");
    }
    printf("]\n");
}

static void printAllocationMatrixNumpyLike(const long long *allocationMatrix, int numberOfSources, int numberOfDestinations) {
    int width = 1;

    for (int sourceIndex = 0; sourceIndex < numberOfSources; sourceIndex++) {
        for (int destinationIndex = 0; destinationIndex < numberOfDestinations; destinationIndex++) {
            long long value = allocationMatrix[(size_t)sourceIndex * (size_t)numberOfDestinations + (size_t)destinationIndex];
            int d = digits_int(value);
            if (d > width) width = d;
        }
    }

    printf("[");
    for (int sourceIndex = 0; sourceIndex < numberOfSources; sourceIndex++) {
        if (sourceIndex > 0) printf(" ");
        printf("[");
        for (int destinationIndex = 0; destinationIndex < numberOfDestinations; destinationIndex++) {
            if (destinationIndex > 0) printf(" ");
            printf("%*lld", width,
                   allocationMatrix[(size_t)sourceIndex * (size_t)numberOfDestinations + (size_t)destinationIndex]);
        }
        printf("]");
        if (sourceIndex < numberOfSources - 1) printf("\n");
    }
    printf("]\n");
}

static long long solveIHW(const Problem *inputProblem, int verbose, long long *allocationMatrix) {
    Problem problem = *inputProblem;

    double *currentSupply = (double*)calloc((size_t)problem.m, sizeof(double));
    double *currentDemand = (double*)calloc((size_t)problem.n, sizeof(double));

    unsigned char *inactiveSource = (unsigned char*)calloc((size_t)problem.m, 1);
    unsigned char *inactiveDestination = (unsigned char*)calloc((size_t)problem.n, 1);

    double *rowPenalty = (double*)calloc((size_t)problem.m, sizeof(double));
    double *columnPenalty = (double*)calloc((size_t)problem.n, sizeof(double));

    double *rowMinimumCost = (double*)calloc((size_t)problem.m, sizeof(double));
    double *columnMinimumCost = (double*)calloc((size_t)problem.n, sizeof(double));

    double *rowFeasibleAllocation = (double*)calloc((size_t)problem.m, sizeof(double));
    double *columnFeasibleAllocation = (double*)calloc((size_t)problem.n, sizeof(double));

    int *rowBestDestination = (int*)calloc((size_t)problem.m, sizeof(int));
    int *columnBestSource = (int*)calloc((size_t)problem.n, sizeof(int));

    double *rowMinAllocation = (double*)calloc((size_t)problem.m, sizeof(double));
    double *columnMinAllocation = (double*)calloc((size_t)problem.n, sizeof(double));

    double *rowWeight = (double*)calloc((size_t)problem.m, sizeof(double));
    double *columnWeight = (double*)calloc((size_t)problem.n, sizeof(double));

    Candidate *rowCandidates = (Candidate*)calloc((size_t)problem.m, sizeof(Candidate));
    Candidate *columnCandidates = (Candidate*)calloc((size_t)problem.n, sizeof(Candidate));

    if (!currentSupply || !currentDemand ||
        !inactiveSource || !inactiveDestination ||
        !rowPenalty || !columnPenalty ||
        !rowMinimumCost || !columnMinimumCost ||
        !rowFeasibleAllocation || !columnFeasibleAllocation ||
        !rowBestDestination || !columnBestSource ||
        !rowMinAllocation || !columnMinAllocation ||
        !rowWeight || !columnWeight ||
        !rowCandidates || !columnCandidates) {
        free(currentSupply);
        free(currentDemand);
        free(inactiveSource);
        free(inactiveDestination);
        free(rowPenalty);
        free(columnPenalty);
        free(rowMinimumCost);
        free(columnMinimumCost);
        free(rowFeasibleAllocation);
        free(columnFeasibleAllocation);
        free(rowBestDestination);
        free(columnBestSource);
        free(rowMinAllocation);
        free(columnMinAllocation);
        free(rowWeight);
        free(columnWeight);
        free(rowCandidates);
        free(columnCandidates);
        return 0;
    }

    for (int sourceIndex = 0; sourceIndex < problem.m; sourceIndex++) {
        currentSupply[sourceIndex] = (double)problem.supply[sourceIndex];
    }

    for (int destinationIndex = 0; destinationIndex < problem.n; destinationIndex++) {
        currentDemand[destinationIndex] = (double)problem.demand[destinationIndex];
    }

    for (size_t cellIndex = 0; cellIndex < (size_t)problem.m * (size_t)problem.n; cellIndex++) {
        allocationMatrix[cellIndex] = 0;
    }

    if (verbose) {
        printf("--- Algoritma RCWMCAM-TOCM ---\n");
        printf("Pilihan Matriks Awal: TCM\n");
        printf("Pilihan Penalti: Inverse Contra-Harmonic Mean\n");
        printf("Pilihan Kriteria: Nilai Terendah\n");
        printf("--- Matriks Biaya Transportasi (TCM) ---\n");
        printProblemCostMatrixLongLongLikeNumpy(&problem);
        printf("--- Matriks Awal yang Digunakan (TCM) ---\n");
        printProblemCostMatrixDoubleLikeNumpy(&problem);
        printf("--- Pasokan (Supply) dan Permintaan (Demand) Awal ---\n");
        printf("Pasokan awal: ");
        printIntArrayLikeNumpyFromLongLong(problem.supply, problem.m);
        printf("\n");
        printf("Permintaan awal: ");
        printIntArrayLikeNumpyFromLongLong(problem.demand, problem.n);
        printf("\n");
        printf("\n==================================================\n\n");
    }

    int iteration = 1;
    const double epsilon = 1e-12;

    while (!allDoubleArrayZeroWithTolerance(currentSupply, problem.m, epsilon) ||
           !allDoubleArrayZeroWithTolerance(currentDemand, problem.n, epsilon)) {
        if (verbose) {
            printf("--- Iterasi %d ---\n", iteration);
        }

        for (int sourceIndex = 0; sourceIndex < problem.m; sourceIndex++) {
            if (inactiveSource[sourceIndex]) {
                rowPenalty[sourceIndex] = 0.0;
                continue;
            }

            rowPenalty[sourceIndex] = calculateRowIchmPenalty(&problem, sourceIndex, inactiveDestination);
        }

        for (int destinationIndex = 0; destinationIndex < problem.n; destinationIndex++) {
            if (inactiveDestination[destinationIndex]) {
                columnPenalty[destinationIndex] = 0.0;
                continue;
            }

            columnPenalty[destinationIndex] = calculateColumnIchmPenalty(&problem, destinationIndex, inactiveSource);
        }

        if (verbose) {
            printf("-> Perhitungan Penalti 1:\n");
            double tempCosts[4096];
            int tempCount = 0;

            for (int sourceIndex = 0; sourceIndex < problem.m; sourceIndex++) {
                if (inactiveSource[sourceIndex]) continue;
                getActiveRowCosts(&problem, sourceIndex, inactiveDestination, tempCosts, &tempCount);
                printf("   - Penalti Baris S%d (costs: ", sourceIndex + 1);
                printDoubleCostsVectorLikeNumpy(tempCosts, tempCount);
                printf("): ");
                printDoublePythonLike(rowPenalty[sourceIndex]);
                printf("\n");
            }

            for (int destinationIndex = 0; destinationIndex < problem.n; destinationIndex++) {
                if (inactiveDestination[destinationIndex]) continue;
                getActiveColumnCosts(&problem, destinationIndex, inactiveSource, tempCosts, &tempCount);
                printf("   - Penalti Kolom D%d (costs: ", destinationIndex + 1);
                printDoubleCostsVectorLikeNumpy(tempCosts, tempCount);
                printf("): ");
                printDoublePythonLike(columnPenalty[destinationIndex]);
                printf("\n");
            }
        }

        int rowCandidateCount = 0;
        int columnCandidateCount = 0;

        if (verbose) {
            printf("\n-> Perhitungan Biaya Min * Alokasi Min:\n");
        }

        for (int sourceIndex = 0; sourceIndex < problem.m; sourceIndex++) {
            if (inactiveSource[sourceIndex]) {
                rowWeight[sourceIndex] = INFINITY;
                continue;
            }

            double minimumCost;
            double feasibleAllocation;
            int bestDestination;

            findBestMinimumCostCellInRow(&problem,
                                         sourceIndex,
                                         inactiveDestination,
                                         currentSupply,
                                         currentDemand,
                                         &minimumCost,
                                         &bestDestination,
                                         &feasibleAllocation);

            rowMinimumCost[sourceIndex] = minimumCost;
            rowBestDestination[sourceIndex] = bestDestination;
            rowFeasibleAllocation[sourceIndex] = feasibleAllocation;
            rowMinAllocation[sourceIndex] = minimumCost * feasibleAllocation;
            rowWeight[sourceIndex] = rowMinAllocation[sourceIndex] * rowPenalty[sourceIndex];

            rowCandidates[rowCandidateCount++] =
                createRowCandidate(sourceIndex,
                                   rowWeight[sourceIndex],
                                   rowMinimumCost[sourceIndex],
                                   rowFeasibleAllocation[sourceIndex],
                                   rowBestDestination[sourceIndex]);

            if (verbose) {
                printf("   - Baris S%d: biaya min=%.1f, kolom dipilih=%d, alokasi min=%lld -> %.1f\n",
                       sourceIndex + 1,
                       minimumCost,
                       bestDestination >= 0 ? bestDestination + 1 : 0,
                       (long long)llround(feasibleAllocation),
                       rowMinAllocation[sourceIndex]);
            }
        }

        for (int destinationIndex = 0; destinationIndex < problem.n; destinationIndex++) {
            if (inactiveDestination[destinationIndex]) {
                columnWeight[destinationIndex] = INFINITY;
                continue;
            }

            double minimumCost;
            double feasibleAllocation;
            int bestSource;

            findBestMinimumCostCellInColumn(&problem,
                                            destinationIndex,
                                            inactiveSource,
                                            currentSupply,
                                            currentDemand,
                                            &minimumCost,
                                            &bestSource,
                                            &feasibleAllocation);

            columnMinimumCost[destinationIndex] = minimumCost;
            columnBestSource[destinationIndex] = bestSource;
            columnFeasibleAllocation[destinationIndex] = feasibleAllocation;
            columnMinAllocation[destinationIndex] = minimumCost * feasibleAllocation;
            columnWeight[destinationIndex] = columnMinAllocation[destinationIndex] * columnPenalty[destinationIndex];

            columnCandidates[columnCandidateCount++] =
                createColumnCandidate(destinationIndex,
                                      columnWeight[destinationIndex],
                                      columnMinimumCost[destinationIndex],
                                      columnFeasibleAllocation[destinationIndex],
                                      columnBestSource[destinationIndex]);

            if (verbose) {
                printf("   - Kolom D%d: biaya min=%.1f, baris dipilih=%d, alokasi min=%lld -> %.1f\n",
                       destinationIndex + 1,
                       minimumCost,
                       bestSource >= 0 ? bestSource + 1 : 0,
                       (long long)llround(feasibleAllocation),
                       columnMinAllocation[destinationIndex]);
            }
        }

        if (verbose) {
            printf("\n-> Perhitungan Penalti 2:\n");

            for (int sourceIndex = 0; sourceIndex < problem.m; sourceIndex++) {
                if (inactiveSource[sourceIndex]) continue;
                printf("   - Baris S%d: %.1f * ", sourceIndex + 1, rowMinAllocation[sourceIndex]);
                printDoublePythonLike(rowPenalty[sourceIndex]);
                printf(" = ");
                printDoublePythonLike(rowWeight[sourceIndex]);
                printf("\n");
            }

            for (int destinationIndex = 0; destinationIndex < problem.n; destinationIndex++) {
                if (inactiveDestination[destinationIndex]) continue;
                printf("   - Kolom D%d: %.1f * ", destinationIndex + 1, columnMinAllocation[destinationIndex]);
                printDoublePythonLike(columnPenalty[destinationIndex]);
                printf(" = ");
                printDoublePythonLike(columnWeight[destinationIndex]);
                printf("\n");
            }
        }

        Candidate bestCandidate =
            chooseBestCandidate(&problem,
                                rowCandidates,
                                rowCandidateCount,
                                columnCandidates,
                                columnCandidateCount,
                                inactiveSource,
                                inactiveDestination,
                                currentSupply,
                                currentDemand,
                                epsilon,
                                verbose);

        if (bestCandidate.idx < 0 || !isfinite(bestCandidate.weight)) {
            break;
        }

        int selectedSource;
        int selectedDestination;

        if (bestCandidate.is_row) {
            selectedSource = bestCandidate.idx;
            selectedDestination = bestCandidate.best_partner;
        } else {
            selectedDestination = bestCandidate.idx;
            selectedSource = bestCandidate.best_partner;
        }

        if (selectedSource < 0 ||
            selectedSource >= problem.m ||
            selectedDestination < 0 ||
            selectedDestination >= problem.n) {
            break;
        }

        double allocationQuantityDouble = getSmallerDouble(currentSupply[selectedSource], currentDemand[selectedDestination]);
        long long allocationQuantity = (long long)llround(allocationQuantityDouble);

        allocationMatrix[(size_t)selectedSource * (size_t)problem.n + (size_t)selectedDestination] = allocationQuantity;

        currentSupply[selectedSource] -= allocationQuantityDouble;
        currentDemand[selectedDestination] -= allocationQuantityDouble;

        if (verbose) {
            printf("\n-> Prioritas dipilih: %s %d dengan penalti 2  ",
                   bestCandidate.is_row ? "Baris" : "Kolom",
                   bestCandidate.idx + 1);
            printDoublePythonLike(bestCandidate.weight);
            printf(".\n");
            printf("   Alokasi %lld unit dari S%d ke D%d.\n",
                   allocationQuantity,
                   selectedSource + 1,
                   selectedDestination + 1);
        }

        if (fabs(currentSupply[selectedSource]) <= epsilon) {
            currentSupply[selectedSource] = 0.0;
            inactiveSource[selectedSource] = 1;
            if (verbose) printf("   Sisa pasokan S%d habis.\n", selectedSource + 1);
        }

        if (fabs(currentDemand[selectedDestination]) <= epsilon) {
            currentDemand[selectedDestination] = 0.0;
            inactiveDestination[selectedDestination] = 1;
            if (verbose) printf("   Sisa permintaan D%d habis.\n", selectedDestination + 1);
        }

        if (verbose) {
            printf("Sisa pasokan: ");
            printIntArrayLikeNumpyFromDouble(currentSupply, problem.m);
            printf("\n");
            printf("Sisa permintaan: ");
            printIntArrayLikeNumpyFromDouble(currentDemand, problem.n);
            printf("\n");
            printf("\n==================================================\n\n");
        }

        iteration++;
    }

    long long totalCost = 0;

    for (int sourceIndex = 0; sourceIndex < problem.m; sourceIndex++) {
        for (int destinationIndex = 0; destinationIndex < problem.n; destinationIndex++) {
            long long allocatedUnit =
                allocationMatrix[(size_t)sourceIndex * (size_t)problem.n + (size_t)destinationIndex];

            if (allocatedUnit != 0) {
                totalCost += allocatedUnit * COST(&problem, sourceIndex, destinationIndex);
            }
        }
    }

    if (verbose) {
        printf("--- HASIL AKHIR ---\n");
        printf("--- Matriks Soal (TCM) ---\n");
        printProblemCostMatrixLongLongLikeNumpy(&problem);
        printf("--- Matriks Alokasi (Final) ---\n");
        printAllocationMatrixNumpyLike(allocationMatrix, problem.m, problem.n);
        printf("Total biaya transportasi dari metode yang dipilih adalah: %lld\n", totalCost);

        if (problem.effective != -1) {
            printf("Total biaya efektif (diberikan) adalah: %lld\n", problem.effective);
            if (totalCost == problem.effective) {
                printf("-> Solusi yang ditemukan adalah optimal.\n");
            } else {
                printf("-> Solusi yang ditemukan adalah solusi layak, tapi bukan solusi optimal.\n");
            }
        }
    }

    free(currentSupply);
    free(currentDemand);
    free(inactiveSource);
    free(inactiveDestination);
    free(rowPenalty);
    free(columnPenalty);
    free(rowMinimumCost);
    free(columnMinimumCost);
    free(rowFeasibleAllocation);
    free(columnFeasibleAllocation);
    free(rowBestDestination);
    free(columnBestSource);
    free(rowMinAllocation);
    free(columnMinAllocation);
    free(rowWeight);
    free(columnWeight);
    free(rowCandidates);
    free(columnCandidates);

    return totalCost;
}

/* =========================
 * Folder listing
 * ========================= */

typedef struct {
    char **items;
    size_t size;
    size_t cap;
} StrList;

static void sl_init(StrList *list) {
    list->items = NULL;
    list->size = 0;
    list->cap = 0;
}

static void sl_free(StrList *list) {
    if (!list) return;

    for (size_t i = 0; i < list->size; i++) {
        free(list->items[i]);
    }

    free(list->items);

    list->items = NULL;
    list->size = 0;
    list->cap = 0;
}

static int sl_push(StrList *list, const char *stringValue) {
    if (list->size == list->cap) {
        size_t newCapacity = list->cap ? list->cap * 2 : 32;

        char **newItems = (char**)realloc(list->items, newCapacity * sizeof(char*));
        if (!newItems) return 0;

        list->items = newItems;
        list->cap = newCapacity;
    }

    size_t length = strlen(stringValue);
    char *copy = (char*)malloc(length + 1);

    if (!copy) return 0;

    memcpy(copy, stringValue, length + 1);
    list->items[list->size++] = copy;

    return 1;
}

static int cmp_strptr(const void *firstPointer, const void *secondPointer) {
    const char *firstString = *(const char* const*)firstPointer;
    const char *secondString = *(const char* const*)secondPointer;

#ifdef _WIN32
    return _stricmp(firstString, secondString);
#else
    return strcasecmp(firstString, secondString);
#endif
}

static int list_txt_files(const char *folderPath, StrList *out) {
    sl_init(out);

#ifdef _WIN32
    char pattern[MAX_PATH];
    snprintf(pattern, sizeof(pattern), "%s\\*.txt", folderPath);

    WIN32_FIND_DATAA fileData;
    HANDLE handle = FindFirstFileA(pattern, &fileData);

    if (handle == INVALID_HANDLE_VALUE) return 0;

    do {
        if (fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;

        if (ends_with_txt(fileData.cFileName)) {
            if (!sl_push(out, fileData.cFileName)) {
                FindClose(handle);
                sl_free(out);
                return 0;
            }
        }
    } while (FindNextFileA(handle, &fileData));

    FindClose(handle);
#else
    DIR *directory = opendir(folderPath);

    if (!directory) return 0;

    struct dirent *entry;

    while ((entry = readdir(directory)) != NULL) {
        if (ends_with_txt(entry->d_name)) {
            if (!sl_push(out, entry->d_name)) {
                closedir(directory);
                sl_free(out);
                return 0;
            }
        }
    }

    closedir(directory);
#endif

    qsort(out->items, out->size, sizeof(char*), cmp_strptr);

    return 1;
}

/* =========================
 * Main modes
 * ========================= */

static void runSingleProblem(const char *filePath, int verboseIfTty) {
    Problem problem;

    if (!readTransportationProblemFile(filePath, &problem)) {
        printf("ERROR: File '%s' tidak ditemukan atau gagal dibaca.\n", filePath);
        return;
    }

    if (!balanceTransportationProblem(&problem)) {
        printf("ERROR: gagal balancing (memory)\n");
        freeProblem(&problem);
        return;
    }

    if (problem.dummy_added) {
        if (problem.dummy_type == 'c') {
            printf("NOTICE: Problem tidak seimbang. Menambahkan dummy destination (kolom) index %d dengan demand %lld.\n",
                   problem.dummy_index + 1,
                   problem.demand[problem.dummy_index]);
        } else {
            printf("NOTICE: Problem tidak seimbang. Menambahkan dummy source (baris) index %d dengan supply %lld.\n",
                   problem.dummy_index + 1,
                   problem.supply[problem.dummy_index]);
        }
    }

    int verbose = verboseIfTty;

    long long *allocationMatrix =
        (long long*)calloc((size_t)problem.m * (size_t)problem.n, sizeof(long long));

    if (!allocationMatrix) {
        printf("ERROR: alloc memory\n");
        freeProblem(&problem);
        return;
    }

    (void)solveIHW(&problem, verbose, allocationMatrix);

    free(allocationMatrix);
    freeProblem(&problem);
}

static void runFolderProblem(const char *folderPath) {
    StrList files;

    if (!list_txt_files(folderPath, &files) || files.size == 0) {
        printf("ERROR: Folder '%s' tidak ditemukan atau tidak ada file .txt.\n", folderPath);
        sl_free(&files);
        return;
    }

    for (size_t fileIndex = 0; fileIndex < files.size; fileIndex++) {
        char fullPath[4096];
        char upperFilename[4096];

        join_path(fullPath, sizeof(fullPath), folderPath, files.items[fileIndex]);
        copy_uppercase(upperFilename, sizeof(upperFilename), files.items[fileIndex]);

        Problem problem;

        if (!readTransportationProblemFile(fullPath, &problem)) {
            printf("%s : ERROR (baca file)\n", upperFilename);
            continue;
        }

        if (!balanceTransportationProblem(&problem)) {
            printf("%s : ERROR (balancing)\n", upperFilename);
            freeProblem(&problem);
            continue;
        }

        if (problem.dummy_added) {
            if (problem.dummy_type == 'c') {
                printf("%s : NOTICE - ditambahkan dummy destination index %d (demand %lld)\n",
                       upperFilename,
                       problem.dummy_index + 1,
                       problem.demand[problem.dummy_index]);
            } else {
                printf("%s : NOTICE - ditambahkan dummy source index %d (supply %lld)\n",
                       upperFilename,
                       problem.dummy_index + 1,
                       problem.supply[problem.dummy_index]);
            }
        }

        long long *allocationMatrix =
            (long long*)calloc((size_t)problem.m * (size_t)problem.n, sizeof(long long));

        if (!allocationMatrix) {
            printf("%s : ERROR (memory)\n", upperFilename);
            freeProblem(&problem);
            continue;
        }

        long long totalCost = solveIHW(&problem, 0, allocationMatrix);

        if (problem.effective != -1) {
            printf("%s : %lld | %lld\n",
                   upperFilename,
                   totalCost,
                   problem.effective);
        } else {
            printf("%s : %lld\n",
                   upperFilename,
                   totalCost);
        }

        free(allocationMatrix);
        freeProblem(&problem);
    }

    sl_free(&files);
}

/* =========================
 * Main program
 * ========================= */

int main(void) {
    printf("--- Program IHW Interaktif (File tunggal, tanpa panggil modul lain) ---\n");
    printf("Mode dan konfigurasi FIX:\n");
    printf("  - Matriks: TCM\n");
    printf("  - Penalti: Inverse Contra-Harmonic Mean (nomor 7)\n");
    printf("  - Kriteria: Lowest (terendah)\n");
    printf("\n");
    printf("Pilih mode program:\n");
    printf("1. Selesaikan satu soal (langkah per langkah)\n");
    printf("2. Selesaikan semua file dalam folder (ringkasan per baris)\n");
    printf("Masukkan pilihan (1/2): ");

    char inputBuffer[4096];

    if (!fgets(inputBuffer, sizeof(inputBuffer), stdin)) {
        return 0;
    }

    rstrip_newline(inputBuffer);

    int selectedMode = atoi(inputBuffer);

    if (selectedMode == 1) {
        printf("Masukkan nama file txt soal (contoh: data.txt): ");

        if (!fgets(inputBuffer, sizeof(inputBuffer), stdin)) {
            return 0;
        }

        rstrip_newline(inputBuffer);

        runSingleProblem(inputBuffer, 1);
    } else if (selectedMode == 2) {
        printf("Masukkan nama folder berisi file soal (contoh: soal_transportasi): ");

        if (!fgets(inputBuffer, sizeof(inputBuffer), stdin)) {
            return 0;
        }

        rstrip_newline(inputBuffer);

        runFolderProblem(inputBuffer);
    } else {
        printf("Pilihan tidak valid. Harap masukkan '1' atau '2'.\n");
    }

    return 0;
}