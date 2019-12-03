/*===========================================================================*\
 *                                                                           *
 *                            ACG Localizer                                  *
 *      Copyright (C) 2011 by Computer Graphics Group, RWTH Aachen           *
 *                           www.rwth-graphics.de                            *
 *                                                                           *
 *---------------------------------------------------------------------------* 
 *  This file is part of ACG Localizer                                       *
 *                                                                           *
 *  ACG Localizer is free software: you can redistribute it and/or modify    *
 *  it under the terms of the GNU General Public License as published by     *
 *  the Free Software Foundation, either version 3 of the License, or        *
 *  (at your option) any later version.                                      *
 *                                                                           *
 *  ACG Localizer is distributed in the hope that it will be useful,         *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of           *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            *
 *  GNU General Public License for more details.                             *
 *                                                                           *
 *  You should have received a copy of the GNU General Public License        *
 *  along with ACG Localizer.  If not, see <http://www.gnu.org/licenses/>.   *
 *                                                                           *
\*===========================================================================*/

#define __STDC_LIMIT_MACROS
#include "query_vector.hh"

//---------------------------------------------------------------------------------------------------------------------------------------------------------------

////
// Actuall localization method for kd-tree based search
////

int query_vector(std::string keylist, int nb_leafs, std::string vw_assignments, int desc_mode, int method, std::string results, std::vector<std::vector<double>> &query_vector)
{
	std::cout << "__________________________________________________________________________________________________________________________" << std::endl;
	std::cout << " -                                                                                                                        - " << std::endl;
	std::cout << " -        Localization method using approximate k-nn search (with flann & one kd-tree).                                   - " << std::endl;
	std::cout << " -                               2011 by Torsten Sattler (tsattler@cs.rwth-aachen.de)                                     - " << std::endl;
	std::cout << " -                                                                                                                        - " << std::endl;
	std::cout << " - usage: acg_localizer_knn list nb_leafs descriptors desc_mode method min_inlier results                                 - " << std::endl;
	std::cout << " - Parameters:                                                                                                            - " << std::endl;
	std::cout << " -  list                                                                                                                  - " << std::endl;
	std::cout << " -     List containing the filenames of all the .key files that should be used as query. It is assumed that the           - " << std::endl;
	std::cout << " -     corresponding images have the same filename except of ending in .jpg.                                              - " << std::endl;
	std::cout << " -                                                                                                                        - " << std::endl;
	std::cout << " -  nb_leafs                                                                                                              - " << std::endl;
	std::cout << " -     The number of leaf nodes to visit for approximate k-nn search.                                                     - " << std::endl;
	std::cout << " -                                                                                                                        - " << std::endl;
	std::cout << " -  descriptors                                                                                                           - " << std::endl;
	std::cout << " -     The assignments assigning descriptors (and 3D points) to visual words, computed by the method                      - " << std::endl;
	std::cout << " -     compute_desc_assignments. The assignments should be computed with the compute_desc_assignments's mode 2 if you set - " << std::endl;
	std::cout << " -     desc_mode to 1 and mode 3 if you set desc_mode to 0                                                                - " << std::endl;
	std::cout << " -                                                                                                                        - " << std::endl;
	std::cout << " -  desc_mode                                                                                                             - " << std::endl;
	std::cout << " -     The way the descriptors in the assignments file are stored (0 = unsigned char, 1 = float).                         - " << std::endl;
	std::cout << " -                                                                                                                        - " << std::endl;
	std::cout << " -  method                                                                                                                - " << std::endl;
	std::cout << " -     0 for FLANN, 1 for ANN, 2 for ANN re-normalized to L2-norm 2. 3 for FLANN using k-means trees.                     - " << std::endl;
	std::cout << " -                                                                                                                        - " << std::endl;
	std::cout << " -  min_inlier                                                                                                            - " << std::endl;
	std::cout << " -     Minimal inlier ratio.                                                                                              - " << std::endl;
	std::cout << " -                                                                                                                        - " << std::endl;
	std::cout << " -  results                                                                                                               - " << std::endl;
	std::cout << " -     The program will write the results of the localization into a text file of name \"results\". It has the following  - " << std::endl;
	std::cout << " -     format, where every line in the file belongs to one query image and has the format                                 - " << std::endl;
	std::cout << " -       #inliers #(correspondences found) (time needed to compute the visual words, in seconds) (time needed to establish- " << std::endl;
	std::cout << " -       the correspondences, in seconds) (time needed for RANSAC, in seconds)                                            - " << std::endl;
	std::cout << "____________________________________________________________________________________________________________________________" << std::endl;

	if (desc_mode != 0 && desc_mode != 1)
	{
		std::cerr << " ERROR: Unknown desc_mode " << desc_mode << ", aborting" << std::endl;
		return -1;
	}

	if (method < 0 || method > 3)
	{
		std::cerr << " ERROR: Unknown method " << method << ", aborting" << std::endl;
		return -1;
	}

	////
	// create and open the output file
	std::ofstream ofs_details(results.c_str(), std::ios::out);

	if (!ofs_details.is_open())
	{
		std::cerr << " Could not write results to " << results << std::endl;
		return 1;
	}

	////
	// load the assignments for the visual words, see localizer_iccv for details

	// ANN, we need to create this here so that we can directly load the data into the tree_descriptors
	// array -> save memory since otherwise we would have to copy it
	ANNkd_tree *kd_tree = 0;
	ANNidxArray indices = 0;
	ANNdistArray distances = 0;
	ANNpointArray tree_descriptors = 0;
	int nb_tree_descriptors = 0;
	ANNcoord *ann_query_descriptor = new ANNcoord[128];

	std::cout << "* Loading and parsing the assignments ... " << std::endl;

	std::vector<OpenMesh::Vec3f> points3D;
	std::vector<float> all_descriptors_float;
	std::vector<uint32_t> point_id_per_descriptor;
	uint32_t nb_non_empty_vw, nb_3D_points, nb_descriptors, nb_cluster;

	{
		std::ifstream ifs(vw_assignments.c_str(), std::ios::in | std::ios::binary);

		if (!ifs)
		{
			std::cerr << " ERROR: Cannot read the visual word assignments " << vw_assignments << std::endl;
			;
			return -1;
		}

		ifs.read((char *)&nb_3D_points, sizeof(uint32_t));
		ifs.read((char *)&nb_cluster, sizeof(uint32_t));
		ifs.read((char *)&nb_non_empty_vw, sizeof(uint32_t));
		ifs.read((char *)&nb_descriptors, sizeof(uint32_t));

		std::cout << " Number of cluster " << nb_cluster << "  Number of non-empty cluster: " << nb_non_empty_vw << " number of points : " << nb_3D_points << " number of descriptors: " << nb_descriptors << std::endl;

		// read the 3D points and their visibility polygons
		points3D.resize(nb_3D_points);
		if (method == 0 || method == 3)
			all_descriptors_float.resize(128 * nb_descriptors);
		else
			tree_descriptors = new ANNpoint[nb_descriptors];

		point_id_per_descriptor.resize(nb_descriptors, 0);

		// load the points
		float *point_data = new float[3];
		for (uint32_t i = 0; i < nb_3D_points; ++i)
		{
			ifs.read((char *)point_data, 3 * sizeof(float));
			for (int j = 0; j < 3; ++j)
				points3D[i][j] = point_data[j];
		}
		delete[] point_data;

		// load the descriptors
		int tmp_int;
		for (uint32_t i = 0; i < nb_descriptors; ++i)
		{
			if (method == 1 || method == 2)
				tree_descriptors[i] = new ANNcoord[128];
			float length = 0.0f;
			for (uint32_t j = 0; j < 128; ++j)
			{
				unsigned char entry_char;
				if (desc_mode == 0 || desc_mode == 2)
				{
					ifs.read((char *)&entry_char, sizeof(unsigned char));
					if (method == 0 || method == 3)
						all_descriptors_float[128 * i + j] = float(entry_char);
					else
					{
						tree_descriptors[i][j] = float(entry_char);
						length += tree_descriptors[i][j] * tree_descriptors[i][j];
					}
				}
				else
				{
					float entry_float;
					ifs.read((char *)&entry_float, sizeof(float));
					if (method == 0 || method == 3)
						all_descriptors_float[128 * i + j] = entry_float;
					else
					{
						tree_descriptors[i][j] = entry_float;
						length += tree_descriptors[i][j] * tree_descriptors[i][j];
					}
				}
			}
			if (method == 2)
			{
				length = sqrt(length);
				for (int j = 0; j < 128; ++j)
					tree_descriptors[i][j] /= length;
			}
		}

		// now we load the assignments of the pairs (point_id, descriptor_id) to the visual words
		for (uint32_t i = 0; i < nb_non_empty_vw; ++i)
		{
			uint32_t id, nb_pairs;
			ifs.read((char *)&id, sizeof(uint32_t));
			ifs.read((char *)&nb_pairs, sizeof(uint32_t));
			uint32_t id_point, id_descriptor;
			for (uint32_t j = 0; j < nb_pairs; ++j)
			{
				ifs.read((char *)&id_point, sizeof(uint32_t));
				ifs.read((char *)&id_descriptor, sizeof(uint32_t));
				point_id_per_descriptor[id_descriptor] = id_point;
			}
		}

		ifs.close();

		std::cout << "  done loading and parsing the assignments " << std::endl;
	}

	////
	// create the search structure

	// FLANN
	visual_words_handler vw_handler;
	vw_handler.set_nb_trees(1);
	vw_handler.set_nb_visual_words(nb_descriptors);
	vw_handler.set_method(std::string("flann"));
	if (method == 0)
		vw_handler.set_flann_type(std::string("randomkd"));
	else if (method == 3)
	{
		vw_handler.set_branching(128);
		vw_handler.set_flann_type(std::string("hkmeans"));
	}
	vw_handler.set_nb_paths(nb_leafs);

	// set the cluster centers (=descriptors) and create the search index
	std::cout << "* Creating the kd-tree ..." << std::endl;
	if (method == 0 || method == 3)
	{
		if (!vw_handler.create_flann_search_index(all_descriptors_float))
		{
			std::cout << " ERROR: Could create the flann search index from the descriptors " << std::endl;
			;
			return -1;
		}
	}
	else
	{
		indices = new ANNidx[2];
		distances = new ANNdist[2];
		nb_tree_descriptors = int(nb_descriptors);

		// create the kd-tree
		kd_tree = new ANNkd_tree(tree_descriptors, nb_tree_descriptors, 128);

		annMaxPtsVisit(nb_leafs);
		all_descriptors_float.clear();
		all_descriptors_float.resize(0);
	}
	std::cout << "  done " << std::endl;

	////
	// now load all the filenames of the query images
	std::vector<std::string> key_filenames;
	key_filenames.clear();
	{
		std::ifstream ifs(keylist.c_str(), std::ios::in);
		std::string tmp_string;

		while (!ifs.eof())
		{
			tmp_string = "";
			ifs >> tmp_string;
			if (!tmp_string.empty())
				key_filenames.push_back(tmp_string);
		}
		ifs.close();
		std::cout << " done loading " << key_filenames.size() << " keyfile names " << std::endl;
	}

	uint32_t nb_keyfiles = key_filenames.size();

	////
	// do the actual localization

	// first initialize some variables used to compute statistics about the number of registered images
	double avrg_reg_time = 0.0;
	double avrg_reject_time = 0.0;
	double avrg_vw_time = 0.0;
	double avrg_nb_features = 0.0;
	double avrg_cor_computation_time_accepted = 0.0;
	double avrg_cor_computation_time_rejected = 0.0;
	double N = 0.0;
	double N_reject = 0.0;
	double mean_inlier_ratio_accepted = 0.0;
	double mean_inlier_ratio_rejected = 0.0;
	double mean_nb_correspondences_accepted = 0.0;
	double mean_nb_correspondences_rejected = 0.0;
	double mean_nb_features_accepted = 0.0;
	double mean_nb_features_rejected = 0.0;
	double vw_time = 0.0;
	double corr_time = 0.0;
	double RANSAC_time = 0.0;
	double avrg_RANSAC_time_registered = 0.0;
	double avrg_RANSAC_time_rejected = 0.0;

	uint32_t registered = 0;
	int mNbNearestNeighbors = 10;

	// store all indices of the 10 nearest neighbors and the squared Euclidean distacnes to them in two large vectors (resized if necessary)
	// preallocated for speed
	std::vector<uint32_t> computed_assignments(100000, 0);
	std::vector<float> computed_squared_distances(100000, 0);

	std::vector<std::vector<double>> query_vectors(nb_keyfiles, std::vector<double>(100000, 0));
	for (uint32_t i = 0; i < nb_keyfiles; ++i, N += 1.0)
	{
		std::cout << std::endl
				  << " --------- " << i + 1 << " / " << nb_keyfiles << " --------- " << std::endl;

		// load the features
		SIFT_loader key_loader;
		std::cout << key_filenames[i] << std::endl;
		key_loader.load_features(key_filenames[i].c_str(), LOWE);

		std::vector<unsigned char *> &descriptors = key_loader.get_descriptors();
		std::vector<SIFT_keypoint> &keypoints = key_loader.get_keypoints();

		uint32_t nb_loaded_keypoints = (uint32_t)keypoints.size();

		// center the keypoints around the center of the image
		// first we need to get the dimensions of the image
		int img_width, img_height;
		std::string jpg_filename(key_filenames[i]);
		jpg_filename.replace(jpg_filename.size() - 3, 3, "jpg");
		exif_reader::open_exif(jpg_filename.c_str());
		img_width = exif_reader::get_image_width();
		img_height = exif_reader::get_image_height();
		exif_reader::close_exif();

		for (uint32_t j = 0; j < nb_loaded_keypoints; ++j)
		{
			keypoints[j].x -= (img_width - 1.0) / 2.0f;
			keypoints[j].y = (img_height - 1.0) / 2.0f - keypoints[j].y;
		}

		std::cout << " loaded " << nb_loaded_keypoints << " descriptors" << std::endl;

		////
		// compute the mNbNearestNeighbors nearest neighbors

		Timer timer;
		timer.Init();
		timer.Start();

		// Resize computed assignments if it is less than 10(number of nn points)
		if (computed_assignments.size() < mNbNearestNeighbors * nb_loaded_keypoints)
		{
			computed_assignments.resize(mNbNearestNeighbors * nb_loaded_keypoints);
			computed_squared_distances.resize(mNbNearestNeighbors * nb_loaded_keypoints);
		}

		vw_handler.set_nb_nearest_neighbors(mNbNearestNeighbors);
		// search for the nearest neighbors
		if (method == 0 || method == 3)
			vw_handler.k_nn_search_flann_ucharv(descriptors, nb_loaded_keypoints, computed_assignments, computed_squared_distances);
		else
		{
			uint32_t index_ = 0;
			for (uint32_t j = 0; j < nb_loaded_keypoints; ++j)
			{
				index_ = 2 * j;
				if (method == 1)
				{
					for (int k = 0; k < 128; ++k)
						ann_query_descriptor[k] = (float)descriptors[j][k];
				}
				else if (method == 2)
				{
					float length = 0.0f;
					for (int k = 0; k < 128; ++k)
					{
						ann_query_descriptor[k] = (float)descriptors[j][k];
						length += ann_query_descriptor[k] * ann_query_descriptor[k];
					}
					length = sqrt(length);
					for (int k = 0; k < 128; ++k)
						ann_query_descriptor[k] /= length;
				}

				kd_tree->annkPriSearch(ann_query_descriptor, 2, indices, distances);
				computed_assignments[index_] = (uint32_t)indices[0];
				computed_assignments[index_ + 1] = (uint32_t)indices[1];
				computed_squared_distances[index_] = distances[0];
				computed_squared_distances[index_ + 1] = distances[1];
			}
		}

		timer.Stop();

		std::cout << " computed 10-nn in " << timer.GetElapsedTimeAsString() << std::endl;
		avrg_vw_time = avrg_vw_time * N / (N + 1.0) + timer.GetElapsedTime() / (N + 1.0);
		avrg_nb_features = avrg_nb_features * N / (N + 1.0) + double(nb_loaded_keypoints) / (N + 1.0);
		vw_time = timer.GetElapsedTime();

		int sigma = 1/4*128;

		// Create Query vector from the assigned 2D points
		// Count 3D-2D mapping from 2D-3D mapping
		std::vector<int> count_vector(nb_descriptors, 0);
		for(size_t i=0; i< nb_loaded_keypoints;i++)
		{
			for(size_t j=0; j<mNbNearestNeighbors;j++)
			{
				uint32_t point3D_index = computed_assignments[mNbNearestNeighbors*i+j];
				count_vector[point3D_index] += 1;
			}
		}

		// for(size_t i=0; i< nb_descriptors;i++)
		// 	std::cout << "Number of 2D features match for 3D point " <<  i  << " is " << count_vector[i] << std::endl;

		std::vector<double> query_vector(nb_descriptors, 0.0); 
		for(size_t i=0; i< nb_loaded_keypoints;i++)
		{
			for(size_t j=0; j<mNbNearestNeighbors;j++)
			{
				uint32_t point3D_index = computed_assignments[mNbNearestNeighbors*i+j];
				float distance = computed_squared_distances[mNbNearestNeighbors*i+j];
				float w_fi = exp(-1*distance*distance)/(sigma*sigma);
				int mNb2dFeatures = count_vector[point3D_index];
				float q_fi = (sqrt(w_fi)*log(nb_descriptors/mNbNearestNeighbors))/mNb2dFeatures;
				query_vector[point3D_index] += q_fi;		
			}	
		}

		for(size_t i=0; i< nb_descriptors;i++)
			std::cout << "Query vector before normalizing for 3D point " <<  i  << " is " << query_vector[i] << std::endl;


		// Calculate the sum for normalizing 
		float sum = std::accumulate(query_vector.begin(), query_vector.end(), 0);
		
		// Normalize the query vector
		for(size_t i=0; i<nb_descriptors;i++)
			query_vector[i] = query_vector[i]/sum;
		
		for(size_t i=0; i< nb_descriptors;i++)
			std::cout << "Query vector after normalizing for 3D point " <<  i  << " is " << query_vector[i] << std::endl;

		query_vectors.push_back(query_vector);
	}

	ofs_details.close();

	std::cout << "#############################" << std::endl;

	////
	// clean-up
	if (method == 1 || method == 2)
	{
		if (kd_tree != 0)
			delete kd_tree;
		kd_tree = 0;

		if (tree_descriptors != 0)
		{
			for (int i = 0; i < nb_tree_descriptors; ++i)
			{
				if (tree_descriptors[i] != 0)
					delete[] tree_descriptors[i];
				tree_descriptors[i] = 0;
			}
			delete[] tree_descriptors;
			tree_descriptors = 0;
		}
	}

	delete[] ann_query_descriptor;
	ann_query_descriptor = 0;

	annClose();
}
