import os
import shutil
import subprocess
import shlex
import io
import logging
import logging.handlers
import requests

class MiniGiraffePipeline:
    def __init__(self, source_folder, destination_folder, log_filename="miniGiraffe.log"):
        # Define global variables
        self.tmp_stdout = "./tmp_stdout.txt"
        self.tmp_stderr = "./tmp_stderr.txt"
        self.logger = self.configure_log(log_filename)

        self.logger.info("***Starting miniGiraffe pipeline***")

        self.source_folder = source_folder
        self.logger.info(f"[Main] Source folder: {self.source_folder}")
        self.destination_folder = destination_folder        
        self.logger.info(f"[Main] Destination folder: {self.destination_folder}")
        self.prepare_destination_folder()

    def configure_log(self, LOG_FILENAME):
        logger = logging.getLogger('miniGiraffe')
        logger.setLevel(logging.DEBUG)
        formatter = logging.Formatter('%(asctime)s %(module)s - %(levelname)s - %(message)s')
        handler = logging.handlers.RotatingFileHandler(
            LOG_FILENAME, maxBytes=268435456, backupCount=50, encoding='utf8'
        )
        handler.setLevel(logging.DEBUG)
        handler.setFormatter(formatter)
        logger.addHandler(handler)
        return logger

    def copy_files(self, source_path, destination_folder):
        try:
            if not os.path.exists(source_path):
                raise FileNotFoundError(f"Source file not found: {source_path}")
            if os.path.isdir(destination_folder):
                destination_path = os.path.join(destination_folder, os.path.basename(source_path))
            if os.path.exists(destination_path):
                raise FileExistsError(f"Destination file already exists: {destination_path}")
            shutil.copy(source_path, destination_path)
            self.logger.info(f"[Copy Files] File copied successfully from {source_path} to {destination_path}")
        except Exception as e:
            self.logger.error(f"[Copy Files] {e}")

    def run_binary(self, binary_path, args=None, env=None):
        try:
            if args is None:
                args = []
            command_list = [binary_path] + args
            command = " ".join(shlex.quote(arg) for arg in command_list)
            self.logger.info(f"[Run Binary] {command}")
            with open(self.tmp_stdout, "w") as fstdout, open(self.tmp_stderr, "w") as fstderr:
                result = subprocess.run(command, stdout=fstdout, stderr=fstderr, text=True, check=True, shell=True, env=env)
            return result
        except Exception as e:
            self.logger.error(f"[Run Binary] {e}")
            return None

    def get_cpu_info(self):
        cpu_info = {}
        try:
            result = subprocess.run(['lscpu'], capture_output=True, text=True, check=True)
            output = result.stdout
            for line in output.splitlines():
                if ':' in line:
                    key, value = line.strip().split(':', 1)
                    cpu_info[key.strip()] = value.strip()
        except Exception as e:
            self.logger.error(f"[CPU Info] {e}")
        return cpu_info

    def prepare_destination_folder(self):
        """
        Create the destination folder if it does not exist.
        If it exists, remove any existing miniGiraffe binaries from the folder.
        """

        try:
            if not os.path.exists(self.destination_folder):
                os.makedirs(self.destination_folder)
                self.logger.info(f"[Main] Created destination folder: {self.destination_folder}")
            else:
                self.logger.warning(f"[Main] Destination folder already exists: {self.destination_folder}")
        except Exception as e:
            self.logger.error(f"[Main] An unexpected error occurred: {e}")

    def remove_existing_files(self, mG_files):
        """
        Remove any existing miniGiraffe binaries from the destination folder.
        """
        try:
            for file in mG_files:
                existing_file_path = os.path.join(self.destination_folder, file)
                if os.path.exists(existing_file_path):
                    os.remove(existing_file_path)
                    self.logger.info(f"[Main] Removed existing file: {existing_file_path}")
        except Exception as e:
            self.logger.error(f"[Main] An unexpected error occurred: {e}")

    def set_machine_folder(self, model):
        """
        Create a machine-specific folder in the destination folder.
        """
        try:
            self.machine_folder = os.path.join(self.destination_folder, model)
            if not os.path.exists(self.machine_folder):
                os.makedirs(self.machine_folder)
                self.logger.info(f"[Main] Created machine folder: {self.machine_folder}")
            else:
                self.logger.info(f"[Main] Machine folder already exists: {self.machine_folder}")
        except Exception as e:
            self.logger.error(f"[Main] An unexpected error occurred: {e}")
            return None

    def run_rsync(self, source, destination, options=None):
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

        try:
            result = subprocess.run(command, capture_output=True, text=True)
            self.logger.info(f"[Rsync] Command: {' '.join(command)}")
            self.logger.info("[Rsync] Command executed successfully.")
            return True
        except Exception as e:
            self.logger.error(f"[Rsync] {e}")
            return False

    def download_zenodo_record(self, record_id):
        """Downloads all files from a Zenodo record.

        Args:
            record_id (int or str): The Zenodo record ID.
        """
        # Zenodo API endpoint for records
        api_url = f"https://zenodo.org/api/records/{record_id}"

        try:
            # Get the record data
            response = requests.get(api_url)
            response.raise_for_status()  # Raise an exception for bad status codes
            record_data = response.json()

            # Extract file information
            files = record_data['files']

            for file_info in files:
                file_url = file_info['links']['self']
                filename = file_info['key']  # Use 'key' for the filename
                filepath = os.path.join(self.destination_folder, filename)

                # Check if the file already exists
                if os.path.exists(filepath):
                    self.logger.warning(f"[Download Zenodo] File already exists: {filepath}")
                    continue

                # Download the file
                self.logger.info(f"[Download Zenodo] Downloading: {filename}")
                with requests.get(file_url, stream=True) as download_response:
                    download_response.raise_for_status()
                    with open(filepath, 'wb') as f:
                        for chunk in download_response.iter_content(chunk_size=8192):
                            f.write(chunk)
                self.logger.info(f"[Download Zenodo] Downloaded: {filename} to {filepath}")

        except requests.exceptions.RequestException as e:
            self.logger.error(f"[Download Zenodo] An error occurred: {e}")
        except KeyError as e:
            self.logger.error(f"[Download Zenodo] Error: Key not found in response: {e}.  Check record ID and API.")
        except Exception as e:
            self.logger.error(f"[Download Zenodo] An unexpected error occurred: {e}")

    def lower_sequence_input(self, sequence_path):
        """
        Lower the sequence input for testing by copying it to the destination folder.
        """
        if not os.path.exists(sequence_path):
            self.logger.error(f"[Lower Sequence Input] Sequence file not found: {sequence_path}")
            raise FileNotFoundError(f"Sequence file not found: {sequence_path}")

        destination_sequence_path = ""
        
        try:
            # Find the last dot
            last_dot_index = sequence_path.rindex('.')
            base_name = sequence_path[:last_dot_index]
            extension = sequence_path[last_dot_index:] # Includes the dot
            destination_sequence_path = base_name + "10p_random" + extension
        except ValueError: # No dot found, so no extension
            base_name = sequence_path
            extension = ""
            destination_sequence_path = base_name + "10p_random" + extension

        if os.path.exists(destination_sequence_path):
            self.logger.warning(f"[Lower Sequence Input] File already exists: {destination_sequence_path}")
            return destination_sequence_path

        binary_path = os.path.join(self.destination_folder, "lower-input")
        output = self.run_binary(binary_path, [sequence_path, destination_sequence_path])
        if output is not None:
            self.logger.info(f"[Lower Sequence Input] Sequence input lowered successfully: {destination_sequence_path}")
            return destination_sequence_path
        else:
            self.logger.error(f"[Lower Sequence Input] Failed to lower sequence input: {sequence_path}")
            exit(-1)
    
    def generate_design_matrix(self, batch_size, cache_size, scheduler, num_threads):
        """
        Generate a systematic design matrix based on the provided factor levels.
        """
        import pyDOE2
        import pandas as pd
        import numpy as np

        factor_level_counts = [
            len(batch_size),
            len(cache_size),
            len(scheduler),
            len(num_threads)
        ]

        # 1. Generate the systematic (non-randomized) design
        design_matrix_indexed = pyDOE2.fullfact(factor_level_counts)

        # Map indexed design to actual factor values
        all_actual_levels = [
            batch_size,
            cache_size,
            scheduler,
            num_threads
        ]
        actual_design_systematic = []
        for row in design_matrix_indexed:
            actual_row = []
            for i, level_index in enumerate(row):
                actual_row.append(all_actual_levels[i][int(level_index)])
            actual_design_systematic.append(actual_row)

        # 2. Randomize the order of runs
        np.random.seed(42)
        random_order_indices = np.random.permutation(len(actual_design_systematic))
        self.full_fact_design = [actual_design_systematic[i] for i in random_order_indices]