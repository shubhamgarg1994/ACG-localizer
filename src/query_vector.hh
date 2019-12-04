
#ifndef QUERY_VECTOR_HH
#define QUERY_VECTOR_HH 

// C++ includes
#include <vector>
#include <set>
#include <map>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <algorithm>
#include <stdint.h>
#include <string>
#include <algorithm>
#include <climits>
#include <float.h>
#include <cmath>
#include <sstream>
#include <stdio.h>
#include <unordered_map>

#include "ann/include/ANN/ANN.h"

// includes for classes dealing with SIFT-features
#include "features/SIFT_loader.hh"
#include "features/visual_words_handler.hh"

// stopwatch
#include "timer.hh"

// math functionality
#include "math/projmatrix.hh"
#include "math/matrix3x3.hh"

// RANSAC
#include "RANSAC.hh"

// exif reader to get the width and height out
// of the exif tags of an image
#include "exif_reader/exif_reader.hh"

// simple vector class for 3D points
#include "OpenMesh_vec/OpenMesh/Core/Geometry/VectorT.hh"

int query_vector(std::string keylist, int nb_leafs, std::string vw_assignments, 
				 int desc_mode, int method, std::string results, 
				 std::vector<std::vector<double>> &query_vector);
#endif