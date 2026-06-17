# Inverse Harmonic Weighted (IHW) Method to Determine the Initial Basic Feasible Solution in the Transportation Problem

> A weighted allocation method for generating an Initial Basic Feasible Solution in balanced transportation problems.

## Project Information

| Field              | Details                                                         |
| ------------------ | --------------------------------------------------------------- |
| Final Project Code | EF234801                                                        |
| Author             | Dana Saputra                                                    |
| Student ID (NRP)   | 5025221003                                                      |
| Advisor            | Dr. Ir. Bilqis Amaliah, S.Kom., M.Kom. |
| Co-advisor         | Ilham Gurat Adillion, S.Kom., M.Eng.        |
| Study Program      | Undergraduate Study Program of Informatics                      |
| Department         | Department of Informatics                                       |
| Faculty            | Faculty of Intelligent Electrical and Informatics Technology    |
| Institution        | Institut Teknologi Sepuluh Nopember                             |

## Overview

The Transportation Problem determines a minimum-cost shipping plan from multiple supply sources to multiple demand destinations while satisfying all supply and demand constraints.

An important stage in solving this problem is constructing an **Initial Basic Feasible Solution (IBFS)**. The quality of the IBFS affects its proximity to the optimal solution and the number of additional optimization iterations required.

This project proposes the **Inverse Harmonic Weighted (IHW)** method as an alternative approach for generating an IBFS for balanced transportation problems.

## Problem Statement

Existing IBFS methods may have one or more of the following limitations:

* Priority decisions are based on limited cost information, such as only the two smallest costs or the maximum cost range.
* Transportation costs and feasible allocation capacities are not always integrated into a single priority measure.
* Tie-breaking procedures may not be sufficiently systematic.
* Results from previous methods are difficult to compare directly because they are often tested on different datasets and metrics.

The proposed method addresses these limitations by combining cost information, feasible allocation capacity, an Inverse Contra-Harmonic Mean penalty, and deterministic tie-breaking rules.

## Proposed Method

IHW combines concepts from the **Row-Column Weighted Minimum-Cost-Allocation Method (RCWMCAM)** and the **Inverse Contra-Harmonic Mean (ICHM)**.

where:

| Component | Description                                                                       |
| --------- | --------------------------------------------------------------------------------- |
| `MC`      | Minimum transportation cost in an active row or column                            |
| `FA`      | Feasible allocation, calculated as the minimum of the remaining supply and demand |
| `ICHM`    | Inverse Contra-Harmonic Mean penalty representing the active cost structure       |
| `W`       | Priority weight used to select the next allocation                                |

The row or column with the **lowest weight** is selected for allocation.

### Allocation Procedure

1. Read the transportation cost matrix, supply values, and demand values.
2. Calculate the minimum cost for every active row and column.
3. Calculate the feasible allocation for each selected minimum-cost cell.
4. Calculate the ICHM penalty for every active row and column.
5. Calculate the weight using `W = MC × FA × ICHM`.
6. Select the candidate with the lowest weight.
7. Allocate units to the corresponding minimum-cost cell.
8. Update the remaining supply and demand.
9. Deactivate fulfilled rows and columns.
10. Repeat until all supply and demand values are satisfied.
11. Calculate the total transportation cost using the original cost matrix.

When candidates have equal weights, the implementation applies tie-breaking based on the lower minimum cost, lower feasible allocation, row priority, and smaller index.

## Implementation

The main IHW method is implemented in **C**. Supporting evaluation and optimal-solution calculations use **Python** and the **Gurobi Optimizer**.

The implementation includes:

* Transportation-problem data parsing
* Supply and demand balance checking
* Dummy source or destination generation when required
* ICHM penalty calculation
* Minimum-cost and feasible-allocation selection
* Tolerance-based floating-point comparisons
* Deterministic candidate selection
* Final allocation and transportation-cost calculation

The experimental study itself uses balanced transportation problems.

## Data and Test Cases

| Category               | Details                         |
| ---------------------- | ------------------------------- |
| Total test cases       | 42                              |
| Literature datasets    | 35                              |
| Synthetic datasets     | 7                               |
| Matrix-size range      | 3×2 to 30×32                    |
| Problem type           | Balanced Transportation Problem |
| Optimal-cost reference | Gurobi Optimizer                |

The synthetic datasets supplement the generally smaller literature datasets by providing larger matrices and more varied transportation costs.

IHW was compared with:

* Vogel’s Approximation Method (VAM)
* Juman and Hoque Method (JHM)
* Total Opportunity Cost Matrix–Minimal Total (TOCM-MT)
* Supply Selection Method (SSM)
* Row-Column Weighted Minimum-Cost-Allocation Method (RCWMCAM)
* Exponential and Inverse Contra-Harmonic Mean (EXP-ICHM)
* Maximum Range Method (MRM)

## Evaluation Metrics

| Metric                                                    | Purpose                                                                      |
| --------------------------------------------------------- | ---------------------------------------------------------------------------- |
| Accuracy Percentage (AP)                                  | Percentage of test cases in which the IBFS directly reaches the optimal cost |
| Deviation Percentage (DP)                                 | Percentage difference between the generated IBFS cost and the optimal cost   |
| Percentage of Correctness (PC)                            | Degree of proximity between the IBFS and the optimal solution                |
| Improvement Percentage (IP)                               | Comparison of the IHW cost against another IBFS method                       |
| Frequency of the Number of Optimal Cost Solutions (FNOCS) | Number of optimal solutions grouped by matrix size                           |
| Optimality Iterations (OI)                                | Number of improvement iterations required after obtaining the IBFS           |

## Key Results

| Method   | Optimal Cases | Accuracy (%) | Average Deviation (%) | Correctness (%) | Optimality Iterations |
| -------- | ------------: | -----------: | --------------------: | --------------: | --------------------: |
| VAM      |         23/42 |        54.76 |                  3.71 |           96.29 |                    63 |
| JHM      |         26/42 |        61.90 |                  3.66 |           96.34 |                    70 |
| TOCM-MT  |         25/42 |        59.52 |                  3.22 |           96.78 |                    69 |
| SSM      |         26/42 |        61.90 |                  3.21 |           96.79 |                    65 |
| RCWMCAM  |         11/42 |        26.19 |                  9.57 |           90.43 |                   103 |
| EXP-ICHM |         27/42 |        64.29 |                  3.41 |           96.59 |                    88 |
| MRM      |         26/42 |        61.90 |                  3.15 |           96.85 |                    62 |
| **IHW**  |     **38/42** |    **90.48** |              **0.51** |       **99.49** |                **32** |

IHW produced the highest accuracy and correctness, the lowest average deviation, and the fewest optimality iterations.

### Improvement against Comparison Methods

The following values show how often IHW produced a lower, higher, or equal IBFS cost across the 42 test cases.

| Compared Method | IHW Lower | IHW Higher | Equal |
| --------------- | --------: | ---------: | ----: |
| VAM             |        19 |          2 |    21 |
| JHM             |        14 |          1 |    27 |
| TOCM-MT         |        16 |          1 |    25 |
| SSM             |        16 |          2 |    24 |
| RCWMCAM         |        31 |          0 |    11 |
| EXP-ICHM        |        15 |          2 |    25 |
| MRM             |        15 |          0 |    27 |

IHW was the only evaluated method to reach the optimal solution for the tested matrices of sizes **5×7, 10×10, 13×13, and 15×15**. However, none of the evaluated methods reached the optimal solution for the **22×21** and **30×32** test cases.

## Conclusion

The IHW method is effective for determining an Initial Basic Feasible Solution in balanced transportation problems. It reached the optimal solution in **38 of 42 test cases**, achieved an accuracy of **90.48%**, produced an average deviation of **0.51%**, and obtained a correctness value of **99.49%**.

With only **32 total optimality iterations**, the solutions generated by IHW were generally closer to the optimal solution than those generated by the comparison methods.

Further development is still required for large transportation matrices, particularly through modifications to cell-selection rules, weight calculations, or integration with solution-improvement methods.
