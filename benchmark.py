import os
import subprocess
import time
import shutil

def run_cmd(cmd):
    if isinstance(cmd, str):
        import shlex
        cmd = shlex.split(cmd)
    result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, shell=False, timeout=10)
    if result.returncode != 0:
        print(f"Command failed: {cmd}")
        print("stdout:", result.stdout)
        print("stderr:", result.stderr)
        exit(1)
    return result.stdout.strip()

print("Building...")
# Configure and build using CMake
if not os.path.exists("build"):
    print(run_cmd(["cmake", "-B", "build", "-S", "."]))
print(run_cmd(["cmake", "--build", "./build", "--config", "Release"]))

exe_path = os.path.join("build", "Release", "git.exe")
if not os.path.exists(exe_path):
    exe_path = os.path.join("build", "git.exe")
if not os.path.exists(exe_path):
    exe_path = os.path.join("build", "Debug", "git.exe")

if not os.path.exists(exe_path):
    print("Executable not found!")
    exit(1)

exe_path = os.path.abspath(exe_path)

test_dir = "benchmark_test_dir"
if os.path.exists(test_dir):
    shutil.rmtree(test_dir)
os.makedirs(test_dir)
os.chdir(test_dir)

print("Initializing git repository...")
run_cmd([exe_path, "init"])

print("Creating 11MB file...")
big_file = "large_file.bin"
with open(big_file, "wb") as f:
    f.write(b"A" * (11 * 1024 * 1024))

print("Starting benchmark for 100 commits...")
parent_sha = None
latencies = []

for i in range(100):
    # modify file slightly
    with open(big_file, "ab") as f:
        f.write(b"mod")
    
    start_time = time.time()
    
    # write tree
    tree_sha = run_cmd([exe_path, "write-tree"])
    
    # commit tree
    msg = f"Commit {i}"
    if parent_sha:
        cmd = [exe_path, "commit-tree", tree_sha, "-p", parent_sha, "-m", msg]
    else:
        cmd = [exe_path, "commit-tree", tree_sha, "-m", msg]
    
    commit_sha = run_cmd(cmd)
    parent_sha = commit_sha
    
    end_time = time.time()
    latencies.append(end_time - start_time)

avg_latency = sum(latencies) / len(latencies)
print(f"Total commits: {len(latencies)}")
print(f"Average latency per commit: {avg_latency:.4f} seconds")
if avg_latency < 0.2:
    print("Benchmark PASSED: Average latency is under 0.2s!")
else:
    print("Benchmark FAILED: Average latency is above 0.2s.")
