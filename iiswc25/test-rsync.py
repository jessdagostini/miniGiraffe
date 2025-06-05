import subprocess

def run_rsync(source, destination, options=None):
    """
    Runs the rsync command.

    Args:
        source (str): Source path.
        destination (str): Destination path.
        options (list, optional): List of rsync options. Defaults to None.

    Returns:
        bool: True if rsync ran successfully, False otherwise.
    """
    command = ["rsync"]
    rsync_options = ["-avz"]
    if options:
        command.extend(options)
    command.extend([source, destination])

    result = subprocess.run(command, capture_output=True, text=True)

    if result.returncode == 0:
        return True
    else:
        print(f"Error running rsync: {result.stderr}")
        return False

# Example usage
source_path = "/lscratch/jessicadagostini/miniGiraffe/intelxeonplatinum8260cpu@240ghz"
destination_path = "/soe/jessicadagostini/miniGiraffe"
rsync_options = ["-avz", "--delete"]

if run_rsync(source_path, destination_path, rsync_options):
    print("Rsync completed successfully.")
else:
    print("Rsync failed.")