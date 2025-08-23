import shutil
import os

def copy_folder_with_parent(source_folder, destination_folder):
    """
    Copies a folder to a new location, preserving the parent directory structure.

    Args:
        source_folder: The path to the folder you want to copy.
        destination_folder: The base path where you want to copy the folder.
                           The source folder's name will be appended to this.
    """
    try:
        # Create the full destination path
        full_destination_path = os.path.join(destination_folder, os.path.basename(source_folder))

        # Check if the source folder exists
        if not os.path.exists(source_folder):
            raise FileNotFoundError(f"Source folder '{source_folder}' does not exist.")

        # Check if destination already exists
        if os.path.exists(full_destination_path):
            raise FileExistsError(f"Destination folder '{full_destination_path}' already exists.")

        # Copy the entire directory tree
        shutil.copytree(source_folder, full_destination_path)

        print(f"Folder '{source_folder}' copied to '{full_destination_path}'")

    except FileNotFoundError as e:
        print(f"Error: {e}")
    except FileExistsError as e:
        print(f"Error: {e}")
    except OSError as e:  # Catch other potential OS errors
        print(f"OS Error: {e}")
    except Exception as e:
        print(f"An unexpected error occurred: {e}")

# --- Example Usage ---
source = "my_folder"
destination = "new_location"

# Create some sample files and folders for demonstration:
os.makedirs(os.path.join(source, "subfolder"), exist_ok=True)
with open(os.path.join(source, "file1.txt"), "w") as f:
    f.write("This is file 1.")
with open(os.path.join(source, "subfolder", "file2.txt"), "w") as f:
    f.write("This is file 2.")

copy_folder_with_parent(source, destination)

# --- Expected Directory Structure After Copy ---
# new_location/
# └── my_folder/
#     ├── file1.txt
#     └── subfolder/
#         └── file2.txt