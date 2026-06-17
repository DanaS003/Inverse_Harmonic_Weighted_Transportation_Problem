import os
from math import isclose

try:
    import gurobipy as gp
    from gurobipy import GRB
except Exception as e:
    print("Error: Gurobi (gurobipy) tidak ditemukan. Pastikan Gurobi dan gurobipy terpasang dan lisensinya aktif.")
    raise

def daftar_file_txt(folder_path):
    """Kembalikan daftar file .txt dalam folder (sorted)."""
    files = [f for f in sorted(os.listdir(folder_path)) if f.lower().endswith('.txt')]
    return files

def baca_format_tp(path_file):
    """
    Membaca file teks berformat:
    - (opsional) baris pertama: "m n"
    - baris berikutnya: matriks biaya (m x n)
    - baris berikutnya: supply (m angka)
    - baris berikutnya: demand (n angka)
    Fungsi toleran terhadap baris kosong/komentar.
    Mengembalikan: C (list of list), S (list), D (list)
    """
    with open(path_file, 'r') as f:
        lines = [ln.strip() for ln in f.readlines()]

    # hapus baris kosong dan komentar
    lines = [ln for ln in lines if ln and not ln.startswith('#')]

    if not lines:
        raise ValueError("File kosong atau hanya berisi komentar/blank.")

    def tokens_of(line):
        return [tok for tok in line.replace(',', ' ').split()]

    # cek apakah baris pertama berisi dims m n
    first_tokens = tokens_of(lines[0])
    m = n = None
    idx = 0
    if len(first_tokens) >= 2 and all(tok.isdigit() for tok in first_tokens[:2]):
        m = int(first_tokens[0]); n = int(first_tokens[1]); idx = 1

    remaining = lines[idx:]

    if m is not None and n is not None:
        if len(remaining) < m + 2:
            raise ValueError("File tidak cukup baris untuk matriks biaya + supply + demand.")
        C = []
        for i in range(m):
            row_toks = tokens_of(remaining[i])
            if len(row_toks) < n:
                raise ValueError(f"Baris matriks biaya ke-{i+1} kurang kolom (diharapkan {n}).")
            row = [float(x) for x in row_toks[:n]]
            C.append(row)
        S = [float(x) for x in tokens_of(remaining[m])[:m]]
        D = [float(x) for x in tokens_of(remaining[m+1])[:n]]
        return C, S, D

    # jika dims tidak ada: deduksi otomatis
    row_tokens = [tokens_of(ln) for ln in remaining]
    counts = [len(rt) for rt in row_tokens]
    if not counts:
        raise ValueError("Tidak ada data numerik ditemukan.")

    from collections import Counter
    c = Counter()
    for ct in counts[:min(10, len(counts))]:
        c[ct] += 1
    if not c:
        raise ValueError("Gagal mendeteksi struktur.")
    common_cols = c.most_common(1)[0][0]

    C = []
    i = 0
    while i < len(row_tokens) and len(row_tokens[i]) == common_cols:
        C.append([float(x) for x in row_tokens[i]])
        i += 1
    if not C:
        raise ValueError("Gagal mendeteksi blok matriks biaya.")
    m = len(C); n = common_cols

    if i >= len(row_tokens):
        raise ValueError("Tidak ditemukan baris supply setelah matriks biaya.")
    if len(row_tokens[i]) < m:
        # fallback: asumsikan dua baris terakhir adalah supply & demand
        if len(row_tokens) >= 2:
            S = [float(x) for x in row_tokens[-2]]
            D = [float(x) for x in row_tokens[-1]]
            return C, S, D
        else:
            raise ValueError("Format tidak sesuai; supply tidak ditemukan.")
    S = [float(x) for x in row_tokens[i][:m]]
    i += 1
    if i >= len(row_tokens):
        raise ValueError("Tidak ditemukan baris demand setelah supply.")
    D = [float(x) for x in row_tokens[i][:n]]
    return C, S, D

def bangun_dan_selesaikan_gurobi(C, S, D, tampilkan_log=True):
    """
    Membangun model Gurobi untuk Transportation Problem (LP).
    Parameter:
      - C, S, D : data problem
      - tampilkan_log (bool): jika True -> model.setParam('OutputFlag', 1) sehingga Gurobi
        akan menampilkan log (termasuk tabel iterasi). Jika False, log disenyapkan.
    Mengembalikan X (solusi matriks original) dan info dict.
    """
    m = len(C)
    n = len(C[0]) if m > 0 else 0
    totalS = sum(S)
    totalD = sum(D)

    # copy struktur & balance jika perlu (dummy row/col dengan cost 0)
    C_mod = [row[:] for row in C]
    S_mod = S[:]
    D_mod = D[:]
    balanced = True
    if not isclose(totalS, totalD, rel_tol=1e-9, abs_tol=1e-9):
        balanced = False
        if totalS > totalD:
            diff = totalS - totalD
            for r in C_mod:
                r.append(0.0)
            D_mod.append(diff)
            n += 1
            print(f"[Info] Balanced: menambahkan dummy demand = {diff:.4g}")
        else:
            diff = totalD - totalS
            C_mod.append([0.0] * n)
            S_mod.append(diff)
            m += 1
            print(f"[Info] Balanced: menambahkan dummy supply = {diff:.4g}")

    # buat model
    model = gp.Model("TP")

    # atur agar Gurobi menampilkan log jika diminta
    if tampilkan_log:
        model.setParam('OutputFlag', 1)
        # biasanya tidak perlu set LogFile; biarkan Gurobi print ke console.
    else:
        model.setParam('OutputFlag', 0)

    # Build variables
    x = {}
    for i in range(m):
        for j in range(n):
            x[i, j] = model.addVar(lb=0.0, vtype=GRB.CONTINUOUS, name=f"x_{i+1}_{j+1}")
    model.update()

    # Objective
    obj = gp.quicksum(C_mod[i][j] * x[i, j] for i in range(m) for j in range(n))
    model.setObjective(obj, GRB.MINIMIZE)

    # Supply constraints
    for i in range(m):
        model.addConstr(gp.quicksum(x[i, j] for j in range(n)) == S_mod[i], name=f"supply_{i+1}")

    # Demand constraints
    for j in range(n):
        model.addConstr(gp.quicksum(x[i, j] for i in range(m)) == D_mod[j], name=f"demand_{j+1}")

    # Optimize (Gurobi akan menampilkan log jika OutputFlag=1)
    model.optimize()

    # Extract info
    status = model.Status
    status_str = {GRB.OPTIMAL: "OPTIMAL",
                  GRB.INFEASIBLE: "INFEASIBLE",
                  GRB.UNBOUNDED: "UNBOUNDED",
                  GRB.TIME_LIMIT: "TIME_LIMIT"}.get(status, str(status))

    def try_attr(obj, name):
        try:
            return getattr(obj, name)
        except Exception:
            try:
                return obj.getAttr(name)
            except Exception:
                return None

    runtime = try_attr(model, 'Runtime')
    iter_count = try_attr(model, 'IterCount')
    node_count = try_attr(model, 'NodeCount')
    sol_count = try_attr(model, 'SolCount')
    objval = None
    try:
        if try_attr(model, 'SolCount') and model.SolCount > 0:
            objval = model.ObjVal
    except Exception:
        objval = None

    # Ambil solusi x untuk ukuran asli (tanpa dummy)
    X = [[0.0 for _ in range(len(C[0]))] for __ in range(len(C))]
    for i in range(len(C)):
        for j in range(len(C[0])):
            try:
                val = x[i, j].X
            except Exception:
                val = 0.0
            X[i][j] = val

    gap = None
    objbound = None
    try:
        gap = model.MIPGap
    except Exception:
        gap = None
    try:
        objbound = model.ObjBound
    except Exception:
        objbound = None

    info = {
        "model": model,
        "status": status_str,
        "runtime": runtime,
        "iter_count": iter_count,
        "node_count": node_count,
        "sol_count": sol_count,
        "objval": objval,
        "gap": gap,
        "objbound": objbound,
        "balanced": balanced
    }

    return X, info

def tampilkan_hasil_interaktif(C, S, D, X, info):
    """Tampilkan hasil optimasi secara rapi ke console (Bahasa Indonesia)."""
    print("\n===== Hasil Optimasi (Gurobi) =====")
    print("Status:", info['status'])
    if info['objval'] is not None:
        print("Objective (total biaya) =", info['objval'])
    if info['runtime'] is not None:
        print(f"Runtime (detik): {info['runtime']:.6g}")
    if info['iter_count'] is not None:
        print("Iterasi (IterCount):", info['iter_count'])
    if info['node_count'] is not None:
        print("NodeCount:", info['node_count'])
    if info['sol_count'] is not None:
        print("Solution Count (SolCount):", info['sol_count'])
    if info['gap'] is not None:
        print("MIP Gap:", info['gap'])
    if info['objbound'] is not None:
        print("Obj Bound:", info['objbound'])
    print("Balanced initially?:", info['balanced'])

    print("\nVariabel keputusan x_ij (hanya nilai > 0 ditampilkan):")
    for i, row in enumerate(X, start=1):
        for j, val in enumerate(row, start=1):
            if abs(val) > 1e-9:
                print(f"  x_{i}{j} = {val:.6g}")

def interaktif_jalankan():
    """
    Fungsi interaktif utama (bahasa Indonesia).
    1) Input folder path
    2) List .txt files
    3) Pilih nomor file
    4) Tanyakan apakah ingin menampilkan log Gurobi (Y/N)
    5) Baca file, jalankan Gurobi, tampilkan hasil
    """
    print("=== TP Gurobi Interaktif ===")
    folder = input("Masukkan path folder yang berisi file .txt (contoh: C:\\data\\x atau ./x): ").strip()
    if not os.path.isdir(folder):
        print("Folder tidak ditemukan. Periksa path dan coba lagi.")
        return

    files = daftar_file_txt(folder)
    if not files:
        print("Tidak ada file .txt di folder tersebut.")
        return

    print("\nDaftar file .txt:")
    for idx, fn in enumerate(files, start=1):
        print(f"{idx}. {fn}")
    choice = input(f"Pilih nomor file (1..{len(files)}): ").strip()
    try:
        k = int(choice) - 1
        if k < 0 or k >= len(files):
            raise ValueError()
    except Exception:
        print("Pilihan tidak valid.")
        return

    # tanya user apakah mau menampilkan log Gurobi
    tampilkan = input("Tampilkan log Gurobi (iterasi) di console? [Y/n] ").strip().lower()
    tampilkan_log = True if tampilkan in ('', 'y', 'yes') else False

    path_file = os.path.join(folder, files[k])
    print(f"\nMembaca file: {path_file}")
    try:
        C, S, D = baca_format_tp(path_file)
    except Exception as e:
        print("Gagal membaca file:", e)
        return

    print("\nMatrix biaya C (c_ij):")
    for row in C:
        print("  ", "  ".join(str(int(x)) if float(x).is_integer() else f"{x:.4g}" for x in row))
    print("Supply S_i:", S)
    print("Demand D_j:", D)

    if tampilkan_log:
        print("\nMenjalankan Gurobi (log akan tampil ke console)...")
    else:
        print("\nMenjalankan Gurobi (log disembunyikan)...")

    X, info = bangun_dan_selesaikan_gurobi(C, S, D, tampilkan_log=tampilkan_log)

    tampilkan_hasil_interaktif(C, S, D, X, info)

    print("\nSelesai.")

if __name__ == "__main__":
    interaktif_jalankan()
