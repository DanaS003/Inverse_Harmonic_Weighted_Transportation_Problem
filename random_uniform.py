from __future__ import annotations

import heapq
import random
from pathlib import Path
from typing import List, Tuple

OUTPUT_DIRECTORY = Path(__file__).resolve().parent
FILE_PREFIX = "RANDOM_"
FILE_EXTENSION = ".TXT"
DELETE_OLD_RANDOM_FILES = True

Matrix = List[List[int]]
Vector = List[int]

RNG = random.SystemRandom()


def ask_int(prompt: str, minimum: int) -> int:
    while True:
        raw = input(f"{prompt}: ").strip()

        try:
            value = int(raw)
        except ValueError:
            print("Masukkan angka bulat yang valid.")
            continue

        if value < minimum:
            print(f"Nilai minimal {minimum}.")
            continue

        return value


def ask_min_max(name: str, minimum_allowed: int) -> Tuple[int, int]:
    while True:
        lower = ask_int(f"{name} minimum", minimum_allowed)
        upper = ask_int(f"{name} maksimum", minimum_allowed)

        if upper < lower:
            print(f"{name} maksimum harus >= {name} minimum.")
            continue

        return lower, upper


def generate_uniform_cost_matrix(
    rows: int,
    cols: int,
    cost_min: int,
    cost_max: int,
    rng: random.SystemRandom,
) -> Matrix:
    return [
        [
            rng.randint(cost_min, cost_max)
            for _ in range(cols)
        ]
        for _ in range(rows)
    ]


def build_composition_table(
    count: int,
    total: int,
    lower: int,
    upper: int,
) -> List[List[int]]:
    ways = [[0] * (total + 1) for _ in range(count + 1)]
    ways[0][0] = 1

    for used_count in range(1, count + 1):
        previous = ways[used_count - 1]
        current = ways[used_count]
        window_sum = 0

        for current_total in range(total + 1):
            add_index = current_total - lower

            if add_index >= 0:
                window_sum += previous[add_index]

            remove_index = current_total - upper - 1

            if remove_index >= 0:
                window_sum -= previous[remove_index]

            current[current_total] = window_sum

    return ways


def weighted_uniform_choice(
    candidates: List[int],
    weights: List[int],
    rng: random.SystemRandom,
) -> int:
    total_weight = sum(weights)

    if total_weight <= 0:
        raise RuntimeError("Tidak ada kandidat random yang feasible.")

    ticket = rng.randrange(total_weight)
    cumulative = 0

    for candidate, weight in zip(candidates, weights):
        cumulative += weight

        if ticket < cumulative:
            return candidate

    raise RuntimeError("Pemilihan random gagal.")


def sample_uniform_bounded_vector(
    count: int,
    total: int,
    lower: int,
    upper: int,
    rng: random.SystemRandom,
) -> Vector:
    if not count * lower <= total <= count * upper:
        raise ValueError(
            f"Total {total} tidak feasible untuk {count} nilai "
            f"dalam rentang {lower}-{upper}."
        )

    ways = build_composition_table(
        count=count,
        total=total,
        lower=lower,
        upper=upper,
    )

    if ways[count][total] == 0:
        raise RuntimeError("Vektor random yang feasible tidak ditemukan.")

    result: Vector = []
    remaining_total = total

    for position in range(count):
        remaining_count = count - position - 1

        feasible_min = max(
            lower,
            remaining_total - remaining_count * upper,
        )

        feasible_max = min(
            upper,
            remaining_total - remaining_count * lower,
        )

        candidates = list(range(feasible_min, feasible_max + 1))

        weights = [
            ways[remaining_count][remaining_total - value]
            for value in candidates
        ]

        selected = weighted_uniform_choice(
            candidates=candidates,
            weights=weights,
            rng=rng,
        )

        result.append(selected)
        remaining_total -= selected

    if remaining_total != 0:
        raise RuntimeError("Pembentukan vektor balance gagal.")

    return result


def generate_balanced_supply_demand(
    rows: int,
    cols: int,
    supply_min: int,
    supply_max: int,
    demand_min: int,
    demand_max: int,
    rng: random.SystemRandom,
) -> Tuple[Vector, Vector]:
    total_min = max(
        rows * supply_min,
        cols * demand_min,
    )

    total_max = min(
        rows * supply_max,
        cols * demand_max,
    )

    if total_min > total_max:
        raise ValueError(
            "Rentang supply dan demand tidak memungkinkan kondisi balance."
        )

    selected_total = rng.randint(total_min, total_max)

    supply = sample_uniform_bounded_vector(
        count=rows,
        total=selected_total,
        lower=supply_min,
        upper=supply_max,
        rng=rng,
    )

    demand = sample_uniform_bounded_vector(
        count=cols,
        total=selected_total,
        lower=demand_min,
        upper=demand_max,
        rng=rng,
    )

    return supply, demand


def generate_random_case(
    rows: int,
    cols: int,
    supply_min: int,
    supply_max: int,
    demand_min: int,
    demand_max: int,
    cost_min: int,
    cost_max: int,
    rng: random.SystemRandom,
) -> Tuple[Matrix, Vector, Vector]:
    cost = generate_uniform_cost_matrix(
        rows=rows,
        cols=cols,
        cost_min=cost_min,
        cost_max=cost_max,
        rng=rng,
    )

    supply, demand = generate_balanced_supply_demand(
        rows=rows,
        cols=cols,
        supply_min=supply_min,
        supply_max=supply_max,
        demand_min=demand_min,
        demand_max=demand_max,
        rng=rng,
    )

    return cost, supply, demand


def calculate_optimal_solution(
    cost: Matrix,
    supply: Vector,
    demand: Vector,
) -> int:
    rows = len(cost)
    cols = len(cost[0])
    total_supply = sum(supply)

    if total_supply != sum(demand):
        raise ValueError(
            "Solusi optimal hanya dihitung untuk supply-demand yang balance."
        )

    source_node = 0
    row_offset = 1
    col_offset = row_offset + rows
    sink_node = col_offset + cols
    node_count = sink_node + 1

    graph: List[List[List[int]]] = [
        [] for _ in range(node_count)
    ]

    def add_edge(
        from_node: int,
        to_node: int,
        capacity: int,
        edge_cost: int,
    ) -> None:
        forward_edge = [
            to_node,
            len(graph[to_node]),
            capacity,
            edge_cost,
        ]

        reverse_edge = [
            from_node,
            len(graph[from_node]),
            0,
            -edge_cost,
        ]

        graph[from_node].append(forward_edge)
        graph[to_node].append(reverse_edge)

    for row_index, row_supply in enumerate(supply):
        add_edge(
            source_node,
            row_offset + row_index,
            row_supply,
            0,
        )

    for row_index in range(rows):
        for col_index in range(cols):
            add_edge(
                row_offset + row_index,
                col_offset + col_index,
                total_supply,
                cost[row_index][col_index],
            )

    for col_index, col_demand in enumerate(demand):
        add_edge(
            col_offset + col_index,
            sink_node,
            col_demand,
            0,
        )

    infinity = 10**30
    potential = [0] * node_count
    total_flow = 0
    optimal_cost = 0

    while total_flow < total_supply:
        distance = [infinity] * node_count
        previous_node = [-1] * node_count
        previous_edge = [-1] * node_count
        distance[source_node] = 0

        priority_queue: List[Tuple[int, int]] = [
            (0, source_node)
        ]

        while priority_queue:
            current_distance, current_node = heapq.heappop(
                priority_queue
            )

            if current_distance != distance[current_node]:
                continue

            for edge_index, edge in enumerate(graph[current_node]):
                to_node, _, remaining_capacity, edge_cost = edge

                if remaining_capacity <= 0:
                    continue

                reduced_cost = (
                    edge_cost
                    + potential[current_node]
                    - potential[to_node]
                )

                new_distance = current_distance + reduced_cost

                if new_distance < distance[to_node]:
                    distance[to_node] = new_distance
                    previous_node[to_node] = current_node
                    previous_edge[to_node] = edge_index

                    heapq.heappush(
                        priority_queue,
                        (new_distance, to_node),
                    )

        if distance[sink_node] == infinity:
            raise RuntimeError(
                "Solusi feasible tidak ditemukan saat menghitung optimal."
            )

        for node in range(node_count):
            if distance[node] < infinity:
                potential[node] += distance[node]

        additional_flow = total_supply - total_flow
        current_node = sink_node

        while current_node != source_node:
            from_node = previous_node[current_node]
            edge_index = previous_edge[current_node]

            if from_node < 0 or edge_index < 0:
                raise RuntimeError(
                    "Jalur augmentasi optimal terputus."
                )

            additional_flow = min(
                additional_flow,
                graph[from_node][edge_index][2],
            )

            current_node = from_node

        path_cost = 0
        current_node = sink_node

        while current_node != source_node:
            from_node = previous_node[current_node]
            edge_index = previous_edge[current_node]
            edge = graph[from_node][edge_index]
            reverse_index = edge[1]

            path_cost += edge[3]
            edge[2] -= additional_flow
            graph[current_node][reverse_index][2] += additional_flow

            current_node = from_node

        total_flow += additional_flow
        optimal_cost += additional_flow * path_cost

    return optimal_cost


def validate_case(
    cost: Matrix,
    supply: Vector,
    demand: Vector,
    rows: int,
    cols: int,
    supply_min: int,
    supply_max: int,
    demand_min: int,
    demand_max: int,
    cost_min: int,
    cost_max: int,
) -> None:
    if len(cost) != rows:
        raise ValueError("Jumlah baris cost tidak sesuai.")

    if any(len(row) != cols for row in cost):
        raise ValueError("Jumlah kolom cost tidak sesuai.")

    if len(supply) != rows:
        raise ValueError("Jumlah supply tidak sesuai.")

    if len(demand) != cols:
        raise ValueError("Jumlah demand tidak sesuai.")

    if any(
        value < cost_min or value > cost_max
        for row in cost
        for value in row
    ):
        raise ValueError("Ada cost di luar rentang.")

    if any(
        value < supply_min or value > supply_max
        for value in supply
    ):
        raise ValueError("Ada supply di luar rentang.")

    if any(
        value < demand_min or value > demand_max
        for value in demand
    ):
        raise ValueError("Ada demand di luar rentang.")

    if sum(supply) != sum(demand):
        raise ValueError("Supply dan demand tidak balance.")


def write_random_case(
    path: Path,
    cost: Matrix,
    supply: Vector,
    demand: Vector,
    optimal_solution: int,
) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary_path = path.with_suffix(path.suffix + ".tmp")

    with temporary_path.open(
        "w",
        encoding="utf-8",
        newline="\n",
    ) as file:
        rows = len(cost)
        cols = len(cost[0])

        file.write(f"{rows} {cols}\n")

        for row in cost:
            file.write(" ".join(map(str, row)) + "\n")

        file.write(" ".join(map(str, supply)) + "\n")
        file.write(" ".join(map(str, demand)) + "\n")
        file.write(f"{optimal_solution}\n")

    temporary_path.replace(path)


def validate_saved_file(
    path: Path,
    rows: int,
    cols: int,
) -> None:
    lines = path.read_text(encoding="utf-8").splitlines()
    expected_line_count = rows + 4

    if len(lines) != expected_line_count:
        raise RuntimeError(
            f"Format {path.name} tidak valid. "
            f"Seharusnya {expected_line_count} baris, "
            f"terbaca {len(lines)}."
        )

    if lines[0].split() != [str(rows), str(cols)]:
        raise RuntimeError(
            f"Header {path.name} tidak sesuai."
        )

    for row_index in range(1, rows + 1):
        if len(lines[row_index].split()) != cols:
            raise RuntimeError(
                f"Jumlah cost baris ke-{row_index} "
                f"di {path.name} tidak sesuai."
            )

    if len(lines[rows + 1].split()) != rows:
        raise RuntimeError(
            f"Jumlah supply di {path.name} tidak sesuai."
        )

    if len(lines[rows + 2].split()) != cols:
        raise RuntimeError(
            f"Jumlah demand di {path.name} tidak sesuai."
        )

    if len(lines[rows + 3].split()) != 1:
        raise RuntimeError(
            f"Baris optimal solution di {path.name} tidak sesuai."
        )

    try:
        optimal_solution = int(lines[rows + 3])
    except ValueError as error:
        raise RuntimeError(
            f"Optimal solution di {path.name} harus berupa angka bulat."
        ) from error

    if optimal_solution < 0:
        raise RuntimeError(
            f"Optimal solution di {path.name} tidak boleh negatif."
        )


def delete_old_random_files() -> None:
    if not DELETE_OLD_RANDOM_FILES:
        return

    OUTPUT_DIRECTORY.mkdir(parents=True, exist_ok=True)

    for path in OUTPUT_DIRECTORY.glob(
        f"{FILE_PREFIX}*{FILE_EXTENSION}"
    ):
        if path.is_file():
            path.unlink()


def main() -> None:
    number_of_questions = ask_int("Jumlah soal", 1)
    rows = ask_int("Jumlah baris/source", 1)
    cols = ask_int("Jumlah kolom/destination", 1)

    while True:
        supply_min, supply_max = ask_min_max("Supply", 1)
        demand_min, demand_max = ask_min_max("Demand", 1)

        total_min = max(
            rows * supply_min,
            cols * demand_min,
        )

        total_max = min(
            rows * supply_max,
            cols * demand_max,
        )

        if total_min <= total_max:
            break

        print(
            "Rentang supply-demand tidak bisa menghasilkan total balance. "
            "Masukkan ulang rentangnya."
        )

    cost_min, cost_max = ask_min_max("Cost", 0)

    delete_old_random_files()

    saved_paths: List[Path] = []

    for question_number in range(1, number_of_questions + 1):
        cost, supply, demand = generate_random_case(
            rows=rows,
            cols=cols,
            supply_min=supply_min,
            supply_max=supply_max,
            demand_min=demand_min,
            demand_max=demand_max,
            cost_min=cost_min,
            cost_max=cost_max,
            rng=RNG,
        )

        validate_case(
            cost=cost,
            supply=supply,
            demand=demand,
            rows=rows,
            cols=cols,
            supply_min=supply_min,
            supply_max=supply_max,
            demand_min=demand_min,
            demand_max=demand_max,
            cost_min=cost_min,
            cost_max=cost_max,
        )

        optimal_solution = calculate_optimal_solution(
            cost=cost,
            supply=supply,
            demand=demand,
        )

        output_path = OUTPUT_DIRECTORY / (
            f"{FILE_PREFIX}{question_number}{FILE_EXTENSION}"
        )

        write_random_case(
            path=output_path,
            cost=cost,
            supply=supply,
            demand=demand,
            optimal_solution=optimal_solution,
        )

        validate_saved_file(
            path=output_path,
            rows=rows,
            cols=cols,
        )

        saved_paths.append(output_path)

    print("\nVALID DAN DISIMPAN")
    print(f"Jumlah file    : {len(saved_paths)}")
    print(f"Folder output  : {OUTPUT_DIRECTORY}")

    for path in saved_paths:
        print(f"File matriks   : {path}")


if __name__ == "__main__":
    main()