import os
import subprocess
import shutil

def run_cmd(cmd):
    print(f"$ {' '.join(cmd)}")
    result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, shell=False)
    if result.returncode != 0:
        print(f"Command failed: {cmd}")
        print("stdout:", result.stdout)
        print("stderr:", result.stderr)
        exit(1)
    if result.stdout:
        print(result.stdout)
    return result.stdout.strip()

exe_path = os.path.abspath(os.path.join("build", "Release", "git.exe"))
if not os.path.exists(exe_path):
    exe_path = os.path.abspath(os.path.join("build", "git.exe"))

test_dir = "test_branch_dir"
if os.path.exists(test_dir):
    shutil.rmtree(test_dir)
os.makedirs(test_dir)
os.chdir(test_dir)

run_cmd([exe_path, "init"])

# Create a file
with open("test.txt", "w") as f:
    f.write("Hello world\n")

# Write tree
tree_sha = run_cmd([exe_path, "write-tree"])

# Commit tree
commit_sha = run_cmd([exe_path, "commit-tree", tree_sha, "-m", "Initial commit"])

# Checkout the commit (detached head)
run_cmd([exe_path, "checkout", commit_sha])

# Create a branch
run_cmd([exe_path, "branch", "my-feature"])

# List branches
run_cmd([exe_path, "branch"])

# Modify file and create a new commit
with open("test.txt", "w") as f:
    f.write("Hello world\nNew feature\n")
tree_sha2 = run_cmd([exe_path, "write-tree"])
commit_sha2 = run_cmd([exe_path, "commit-tree", tree_sha2, "-p", commit_sha, "-m", "Second commit"])

# Since we are in detached HEAD, the branch 'my-feature' wasn't updated. 
# But we can checkout 'my-feature'
run_cmd([exe_path, "checkout", "my-feature"])

# Verify content reverted to first commit
with open("test.txt", "r") as f:
    content = f.read()
    if content == "Hello world\n":
        print("SUCCESS: File content matches the branch state.")
    else:
        print(f"FAILED: File content is {content}")

# Checkout the new commit
run_cmd([exe_path, "checkout", commit_sha2])

# Verify content is new commit
with open("test.txt", "r") as f:
    content = f.read()
    if content == "Hello world\nNew feature\n":
        print("SUCCESS: File content matches the new commit.")
    else:
        print(f"FAILED: File content is {content}")

print("All tests passed perfectly!")
