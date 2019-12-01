1. Download data folder from https://drive.google.com/drive/folders/1EFfCJPzxKaLS-eXvqNeS6NZzMCFyDyFE?usp=sharing
2. Run the following command to output bundler files from colmap database.db. Outputs the key files for all the images
     python export_to_bundler.py --database_path ../data/database.db --image_path ../data/images/ --output_path ../data/bundle_files
3. Run the following command from bin directory inside build folder to extract the relevant information from the bundler file
    ./Bundle2Info ../../data/bundler ../../data/images.txt ../../data/BundleInfo.txt
4. Download 100K vocabulary from https://www.graphics.rwth-aachen.de/media/resource_files/generic_vocabulary_100k.zip  
5. Run the following command from bin directory inside build folder to compute visual words assignments for each of the 3D points
./compute_desc_assignments ../../data/BundleInfo.txt 1 100000 ../../data/markt_paris_gpu_sift_100k.cluster ../../data/assignments.txt 6 1 0