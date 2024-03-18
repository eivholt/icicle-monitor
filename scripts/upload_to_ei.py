import os
import sys
import edgeimpulse as ei
from tqdm import tqdm

def upload_files(folder_path, project_api_key):
    # Initialize uploader
    ei.API_KEY = project_api_key

    # Upload the entire directory (including the labels.info file)
    response = ei.experimental.data.upload_plain_directory(
        directory=folder_path
    )

    # Check to make sure there were no failures
    assert len(response.fails) == 0, "Could not upload some files"

    # Save the sample IDs, as we will need these to retrieve file information and delete samples
    ids = []
    for sample in response.successes:
        ids.append(sample.sample_id)

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python script.py <folder_path> <project_api_key>")
        sys.exit(1)

    folder_path = sys.argv[1]
    project_api_key = sys.argv[2]

    upload_files(folder_path, project_api_key)
