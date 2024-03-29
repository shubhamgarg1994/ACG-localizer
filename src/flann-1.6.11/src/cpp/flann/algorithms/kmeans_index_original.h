/***********************************************************************
 * Software License Agreement (BSD License)
 *
 * Copyright 2008-2009  Marius Muja (mariusm@cs.ubc.ca). All rights reserved.
 * Copyright 2008-2009  David G. Lowe (lowe@cs.ubc.ca). All rights reserved.
 *
 * THE BSD LICENSE
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *************************************************************************/

#ifndef KMEANSTREE_H
#define KMEANSTREE_H

#include <algorithm>
#include <string>
#include <map>
#include <cassert>
#include <limits>
#include <cmath>

#include "flann/general.h"
#include "flann/algorithms/nn_index.h"
#include "flann/algorithms/dist.h"
#include "flann/util/matrix.h"
#include "flann/util/result_set.h"
#include "flann/util/heap.h"
#include "flann/util/allocator.h"
#include "flann/util/random.h"
#include "flann/util/saving.h"


namespace flann
{

struct KMeansIndexParams : public IndexParams
{
    KMeansIndexParams(int branching_ = 32, int iterations_ = 11,
                      flann_centers_init_t centers_init_ = FLANN_CENTERS_RANDOM, float cb_index_ = 0.2 ) :
        IndexParams(FLANN_INDEX_KMEANS),
        branching(branching_),
        iterations(iterations_),
        centers_init(centers_init_),
        cb_index(cb_index_) {}

    int branching;             // branching factor (for kmeans tree)
    int iterations;            // max iterations to perform in one kmeans clustering (kmeans tree)
    flann_centers_init_t centers_init;          // algorithm used for picking the initial cluster centers for kmeans tree
    float cb_index;            // cluster boundary index. Used when searching the kmeans tree

    void fromParameters(const FLANNParameters& p)
    {
        assert(p.algorithm==FLANN_INDEX_KMEANS);
        branching = p.branching;
        iterations = p.iterations;
        centers_init = p.centers_init;
        cb_index = p.cb_index;
    }

    void toParameters(FLANNParameters& p) const
    {
        p.algorithm = FLANN_INDEX_KMEANS;
        p.branching = branching;
        p.iterations = iterations;
        p.centers_init = centers_init;
        p.cb_index = cb_index;
    }

    void print() const
    {
        logger.info("Index type: %d\n",(int)algorithm);
        logger.info("Branching: %d\n", branching);
        logger.info("Iterations: %d\n", iterations);
        logger.info("Centres initialisation: %d\n", centers_init);
        logger.info("Cluster boundary weight: %g\n", cb_index);
    }

};


/**
 * Hierarchical kmeans index
 *
 * Contains a tree constructed through a hierarchical kmeans clustering
 * and other information for indexing a set of points for nearest-neighbour matching.
 */
template <typename Distance>
class KMeansIndex : public NNIndex<Distance>
{
public:
    typedef typename Distance::ElementType ElementType;
    typedef typename Distance::ResultType DistanceType;
private:
    /**
     * The branching factor used in the hierarchical k-means clustering
     */
    int branching;

    /**
     * Maximum number of iterations to use when performing k-means
     * clustering
     */
    int max_iter;

    /**
     * Cluster border index. This is used in the tree search phase when determining
     * the closest cluster to explore next. A zero value takes into account only
     * the cluster centres, a value greater then zero also take into account the size
     * of the cluster.
     */
    float cb_index;

    /**
     * The dataset used by this index
     */
    const Matrix<ElementType> dataset;

    const KMeansIndexParams index_params;

    /**
     * Number of features in the dataset.
     */
    size_t size_;

    /**
     * Length of each feature.
     */
    size_t veclen_;


    /**
     * Struture representing a node in the hierarchical k-means tree.
     */
    struct KMeansNode
    {
        /**
         * The cluster center.
         */
        DistanceType* pivot;
        /**
         * The cluster radius.
         */
        DistanceType radius;
        /**
         * The cluster mean radius.
         */
        DistanceType mean_radius;
        /**
         * The cluster variance.
         */
        DistanceType variance;
        /**
         * The cluster size (number of points in the cluster)
         */
        int size;
        /**
         * Child nodes (only for non-terminal nodes)
         */
        KMeansNode** childs;
        /**
         * Node points (only for terminal nodes)
         */
        int* indices;
        /**
         * Level
         */
        int level;
    };
    typedef KMeansNode* KMeansNodePtr;



    /**
     * Alias definition for a nicer syntax.
     */
    typedef BranchStruct<KMeansNodePtr, DistanceType> BranchSt;


    /**
     * The root node in the tree.
     */
    KMeansNodePtr root;

    /**
     *  Array of indices to vectors in the dataset.
     */
    int* indices;


    /**
     * The distance
     */
    Distance distance;



    /**
     * Pooled memory allocator.
     *
     * Using a pooled memory allocator is more efficient
     * than allocating memory directly when there is a large
     * number small of memory allocations.
     */
    PooledAllocator pool;

    /**
     * Memory occupied by the index.
     */
    int memoryCounter;


    typedef void (KMeansIndex::* centersAlgFunction)(int, int*, int, int*, int&);

    /**
     * The function used for choosing the cluster centers.
     */
    centersAlgFunction chooseCenters;



    /**
     * Chooses the initial centers in the k-means clustering in a random manner.
     *
     * Params:
     *     k = number of centers
     *     vecs = the dataset of points
     *     indices = indices in the dataset
     *     indices_length = length of indices vector
     *
     */
    void chooseCentersRandom(int k, int* indices, int indices_length, int* centers, int& centers_length)
    {
        UniqueRandom r(indices_length);

        int index;
        for (index=0; index<k; ++index) {
            bool duplicate = true;
            int rnd;
            while (duplicate) {
                duplicate = false;
                rnd = r.next();
                if (rnd<0) {
                    centers_length = index;
                    return;
                }

                centers[index] = indices[rnd];

                for (int j=0; j<index; ++j) {
                    float sq = distance(dataset[centers[index]], dataset[centers[j]], dataset.cols);
                    if (sq<1e-16) {
                        duplicate = true;
                    }
                }
            }
        }

        centers_length = index;
    }


    /**
     * Chooses the initial centers in the k-means using Gonzales' algorithm
     * so that the centers are spaced apart from each other.
     *
     * Params:
     *     k = number of centers
     *     vecs = the dataset of points
     *     indices = indices in the dataset
     * Returns:
     */
    void chooseCentersGonzales(int k, int* indices, int indices_length, int* centers, int& centers_length)
    {
        int n = indices_length;

        int rnd = rand_int(n);
        assert(rnd >=0 && rnd < n);

        centers[0] = indices[rnd];

        int index;
        for (index=1; index<k; ++index) {

            int best_index = -1;
            float best_val = 0;
            for (int j=0; j<n; ++j) {
                float dist = distance(dataset[centers[0]],dataset[indices[j]],dataset.cols);
                for (int i=1; i<index; ++i) {
                    float tmp_dist = distance(dataset[centers[i]],dataset[indices[j]],dataset.cols);
                    if (tmp_dist<dist) {
                        dist = tmp_dist;
                    }
                }
                if (dist>best_val) {
                    best_val = dist;
                    best_index = j;
                }
            }
            if (best_index!=-1) {
                centers[index] = indices[best_index];
            }
            else {
                break;
            }
        }
        centers_length = index;
    }


    /**
     * Chooses the initial centers in the k-means using the algorithm
     * proposed in the KMeans++ paper:
     * Arthur, David; Vassilvitskii, Sergei - k-means++: The Advantages of Careful Seeding
     *
     * Implementation of this function was converted from the one provided in Arthur's code.
     *
     * Params:
     *     k = number of centers
     *     vecs = the dataset of points
     *     indices = indices in the dataset
     * Returns:
     */
    void chooseCentersKMeanspp(int k, int* indices, int indices_length, int* centers, int& centers_length)
    {
        int n = indices_length;

        double currentPot = 0;
        DistanceType* closestDistSq = new DistanceType[n];

        // Choose one random center and set the closestDistSq values
        int index = rand_int(n);
        assert(index >=0 && index < n);
        centers[0] = indices[index];

        for (int i = 0; i < n; i++) {
            closestDistSq[i] = distance(dataset[indices[i]], dataset[indices[index]], dataset.cols);
            currentPot += closestDistSq[i];
        }


        const int numLocalTries = 1;

        // Choose each center
        int centerCount;
        for (centerCount = 1; centerCount < k; centerCount++) {

            // Repeat several trials
            double bestNewPot = -1;
            int bestNewIndex;
            for (int localTrial = 0; localTrial < numLocalTries; localTrial++) {

                // Choose our center - have to be slightly careful to return a valid answer even accounting
                // for possible rounding errors
                double randVal = rand_double(currentPot);
                for (index = 0; index < n-1; index++) {
                    if (randVal <= closestDistSq[index]) break;
                    else randVal -= closestDistSq[index];
                }

                // Compute the new potential
                double newPot = 0;
                for (int i = 0; i < n; i++) newPot += std::min( distance(dataset[indices[i]], dataset[indices[index]], dataset.cols), closestDistSq[i] );

                // Store the best result
                if ((bestNewPot < 0)||(newPot < bestNewPot)) {
                    bestNewPot = newPot;
                    bestNewIndex = index;
                }
            }

            // Add the appropriate center
            centers[centerCount] = indices[bestNewIndex];
            currentPot = bestNewPot;
            for (int i = 0; i < n; i++) closestDistSq[i] = std::min( distance(dataset[indices[i]], dataset[indices[bestNewIndex]], dataset.cols), closestDistSq[i] );
        }

        centers_length = centerCount;

        delete[] closestDistSq;
    }



public:


    flann_algorithm_t getType() const
    {
        return FLANN_INDEX_KMEANS;
    }

    /**
     * Index constructor
     *
     * Params:
     *          inputData = dataset with the input features
     *          params = parameters passed to the hierarchical k-means algorithm
     */
    KMeansIndex(const Matrix<ElementType>& inputData, const KMeansIndexParams& params = KMeansIndexParams(),
                Distance d = Distance())
        : dataset(inputData), index_params(params), root(NULL), indices(NULL), distance(d)
    {
        memoryCounter = 0;

        size_ = dataset.rows;
        veclen_ = dataset.cols;

        branching = params.branching;
        max_iter = params.iterations;
        if (max_iter<0) {
            max_iter = (std::numeric_limits<int>::max)();
        }
        flann_centers_init_t centersInit = params.centers_init;

        if (centersInit==FLANN_CENTERS_RANDOM) {
            chooseCenters = &KMeansIndex::chooseCentersRandom;
        }
        else if (centersInit==FLANN_CENTERS_GONZALES) {
            chooseCenters = &KMeansIndex::chooseCentersGonzales;
        }
        else if (centersInit==FLANN_CENTERS_KMEANSPP) {
            chooseCenters = &KMeansIndex::chooseCentersKMeanspp;
        }
        else {
            throw FLANNException("Unknown algorithm for choosing initial centers.");
        }
        cb_index = 0.4;

    }


    /**
     * Index destructor.
     *
     * Release the memory used by the index.
     */
    virtual ~KMeansIndex()
    {
        if (root != NULL) {
            free_centers(root);
        }
        if (indices!=NULL) {
            delete[] indices;
        }
    }

    /**
     *  Returns size of index.
     */
    size_t size() const
    {
        return size_;
    }

    /**
     * Returns the length of an index feature.
     */
    size_t veclen() const
    {
        return veclen_;
    }


    void set_cb_index( float index)
    {
        cb_index = index;
    }


    /**
     * Computes the inde memory usage
     * Returns: memory used by the index
     */
    int usedMemory() const
    {
        return pool.usedMemory+pool.wastedMemory+memoryCounter;
    }

    /**
     * Builds the index
     */
    void buildIndex()
    {
        if (branching<2) {
            throw FLANNException("Branching factor must be at least 2");
        }

        indices = new int[size_];
        for (size_t i=0; i<size_; ++i) {
            indices[i] = i;
        }

        root = pool.allocate<KMeansNode>();
        computeNodeStatistics(root, indices, size_);
        computeClustering(root, indices, size_, branching,0);
    }


    void saveIndex(FILE* stream)
    {
        save_value(stream, branching);
        save_value(stream, max_iter);
        save_value(stream, memoryCounter);
        save_value(stream, cb_index);
        save_value(stream, *indices, size_);

        save_tree(stream, root);
    }


    void loadIndex(FILE* stream)
    {
        load_value(stream, branching);
        load_value(stream, max_iter);
        load_value(stream, memoryCounter);
        load_value(stream, cb_index);
        if (indices!=NULL) {
            delete[] indices;
        }
        indices = new int[size_];
        load_value(stream, *indices, size_);

        if (root!=NULL) {
            free_centers(root);
        }
        load_tree(stream, root);
    }


    /**
     * Find set of nearest neighbors to vec. Their indices are stored inside
     * the result object.
     *
     * Params:
     *     result = the result object in which the indices of the nearest-neighbors are stored
     *     vec = the vector for which to search the nearest neighbors
     *     searchParams = parameters that influence the search algorithm (checks, cb_index)
     */
    void findNeighbors(ResultSet<DistanceType>& result, const ElementType* vec, const SearchParams& searchParams)
    {

        int maxChecks = searchParams.checks;

        if (maxChecks==FLANN_CHECKS_UNLIMITED) {
            findExactNN(root, result, vec);
        }
        else {
            // Priority queue storing intermediate branches in the best-bin-first search
            Heap<BranchSt>* heap = new Heap<BranchSt>(size_);

            int checks = 0;

            findNN(root, result, vec, checks, maxChecks, heap);

            BranchSt branch;
            while (heap->popMin(branch) && (checks<maxChecks || !result.full())) {
                KMeansNodePtr node = branch.node;
                findNN(node, result, vec, checks, maxChecks, heap);
            }
            assert(result.full());

            delete heap;
        }

    }


    /**
     * Clustering function that takes a cut in the hierarchical k-means
     * tree and return the clusters centers of that clustering.
     * Params:
     *     numClusters = number of clusters to have in the clustering computed
     * Returns: number of cluster centers
     */
    int getClusterCenters(Matrix<DistanceType>& centers)
    {
        int numClusters = centers.rows;
        if (numClusters<1) {
            throw FLANNException("Number of clusters must be at least 1");
        }

        float variance;
        KMeansNodePtr* clusters = new KMeansNodePtr[numClusters];

        int clusterCount = getMinVarianceClusters(root, clusters, numClusters, variance);

        //         logger.info("Clusters requested: %d, returning %d\n",numClusters, clusterCount);


        for (int i=0; i<clusterCount; ++i) {
            DistanceType* center = clusters[i]->pivot;
            for (size_t j=0; j<veclen_; ++j) {
                centers[i][j] = center[j];
            }
        }
        delete[] clusters;

        return clusterCount;
    }

    const IndexParams* getParameters() const
    {
        return &index_params;
    }

    /**
      * For the data points used to construct the tree, compute the corresponding cluster
      * centers on level L of the tree (level 0 is the root node).
      * Params:
      *    level_L = the level L 
      *    centerIds = array in which we store the ids of the cluster centers, starting with id 0
      *                the size of the array has to correspond to the number of datapoints used
      *                to construct the hkmeans tree.
      * Return: The maximum level of the tree
      * 
      * Written by Torsten Sattler.
      */
    int getClusterCentersOnLevelL( int level_L, int* &clusterIds ) const
    {

      ////
      // first determine the maximum level of the tree
      // we do so by traversing the tree (in breadth first order
      int max_level = -1;
      
      std::list< KMeansNodePtr > fifo;
      fifo.clear();
      
      if( root == NULL )
        return -2;
      
      fifo.push_back( root );
      
      while (!fifo.empty()) {
        KMeansNodePtr node = fifo.front();
        
        if( node != 0 )
        {
          max_level = std::max( max_level, node->level );
          
          if( node->childs != 0 ) {
            for (int i=0; i<branching; ++i) {
              fifo.push_back(node->childs[i]);
            }
          }
        }
        
        node = 0;
        
        fifo.pop_front();
      }
      
      ////
      // Some statistics
      int nb_nodes = 1; // index 0 is the root
      int mult = 1;
      for( int i=1; i<max_level; ++i )
      {
        mult *= branching;
        nb_nodes += mult;
      }
      std::cout << " number nodes: " << nb_nodes << " for " << max_level << " levels and branching " << branching << std::endl;
      int nb_nodes_before_L = 1;
      mult = 1;
      for( int i=1; i<level_L; ++i )
      {
        mult *= branching;
        nb_nodes_before_L += mult;
      }
      std::cout << nb_nodes_before_L << " nodes before level " << level_L << ", with branching " << branching << std::endl;

      ////
      // now we need to compute the assignments of the indices stored in the nodes of the tree to the higher level L
      // we do so by traversing the tree in breadth first search, than doing a search back using 
      // array indexing (i.e. the children of node with id i are at branching * i + 1 ... branching * i + branching
      // the parent of node i is floor( (i-1) / branching )
      std::list< std::pair< KMeansNodePtr, int > > fifo_ids;
      fifo_ids.clear();
      
      fifo_ids.push_back( std::make_pair< KMeansNodePtr, int >( root, 0 ) );
      
      while (!fifo_ids.empty()) {
        std::pair< KMeansNodePtr, int > node;
        node.first = fifo_ids.front().first;
        node.second = fifo_ids.front().second;
        
        if( node.first != 0 )
        {
          if( node.first->childs != 0 ) {
            for (int i=0; i<branching; ++i) {
              fifo_ids.push_back( std::make_pair< KMeansNodePtr, int >( node.first->childs[i], node.second * branching + i + 1 ) );
            }
          }
          else {
            // we are at a leaf, so we need to get the index of its parent at level L
            int id_of_parent_at_level_L = node.second;
            int current_level = node.first->level;
            
            while( current_level < level_L ) {
                id_of_parent_at_level_L = id_of_parent_at_level_L * branching + 1;
                current_level += 1;
            }
            
            while( current_level > level_L ) {
              id_of_parent_at_level_L = (int) floor( ( id_of_parent_at_level_L - 1) / branching );
              current_level -= 1;
            }
            
            // it might happen that the tree is not balanced and that we cannot descend to the desired level
            // in this case, when at level k, we assign the node on level k to the first node of its 
            // subtree on level L
            if( current_level < level_L ) {
                id_of_parent_at_level_L = id_of_parent_at_level_L * branching + 1;
                current_level += 1;
            }
            
            id_of_parent_at_level_L -= nb_nodes_before_L;
            
            // now store this id for all indices in this leaf
            for( int i=0; i<node.first->size; ++i ) {
              clusterIds[ node.first->indices[ i ] ] = id_of_parent_at_level_L;
            }
            
          }
        }
        
        
        fifo_ids.pop_front();
      }
      
      return max_level;
    }

private:


    void save_tree(FILE* stream, KMeansNodePtr node)
    {
        save_value(stream, *node);
        save_value(stream, *(node->pivot), veclen_);
        if (node->childs==NULL) {
            int indices_offset = node->indices - indices;
            save_value(stream, indices_offset);
        }
        else {
            for(int i=0; i<branching; ++i) {
                save_tree(stream, node->childs[i]);
            }
        }
    }


    void load_tree(FILE* stream, KMeansNodePtr& node)
    {
        node = pool.allocate<KMeansNode>();
        load_value(stream, *node);
        node->pivot = new DistanceType[veclen_];
        load_value(stream, *(node->pivot), veclen_);
        if (node->childs==NULL) {
            int indices_offset;
            load_value(stream, indices_offset);
            node->indices = indices + indices_offset;
        }
        else {
            node->childs = pool.allocate<KMeansNodePtr>(branching);
            for(int i=0; i<branching; ++i) {
                load_tree(stream, node->childs[i]);
            }
        }
    }


    /**
     * Helper function
     */
    void free_centers(KMeansNodePtr node)
    {
        delete[] node->pivot;
        if (node->childs!=NULL) {
            for (int k=0; k<branching; ++k) {
                free_centers(node->childs[k]);
            }
        }
    }

    /**
     * Computes the statistics of a node (mean, radius, variance).
     *
     * Params:
     *     node = the node to use
     *     indices = the indices of the points belonging to the node
     */
    void computeNodeStatistics(KMeansNodePtr node, int* indices, int indices_length)
    {

        DistanceType radius = 0;
        DistanceType variance = 0;
        DistanceType* mean = new DistanceType[veclen_];
        memoryCounter += veclen_*sizeof(DistanceType);

        memset(mean,0,veclen_*sizeof(float));

        for (size_t i=0; i<size_; ++i) {
            ElementType* vec = dataset[indices[i]];
            for (size_t j=0; j<veclen_; ++j) {
                mean[j] += vec[j];
            }
            variance += distance(vec, ZeroIterator<ElementType>(), veclen_);
        }
        for (size_t j=0; j<veclen_; ++j) {
            mean[j] /= size_;
        }
        variance /= size_;
        variance -= distance(mean, ZeroIterator<ElementType>(), veclen_);

        DistanceType tmp = 0;
        for (int i=0; i<indices_length; ++i) {
            tmp = distance(mean, dataset[indices[i]], veclen_);
            if (tmp>radius) {
                radius = tmp;
            }
        }

        node->variance = variance;
        node->radius = radius;
        node->pivot = mean;
    }


    /**
     * The method responsible with actually doing the recursive hierarchical
     * clustering
     *
     * Params:
     *     node = the node to cluster
     *     indices = indices of the points belonging to the current node
     *     branching = the branching factor to use in the clustering
     *
     * TODO: for 1-sized clusters don't store a cluster center (it's the same as the single cluster point)
     */
    void computeClustering(KMeansNodePtr node, int* indices, int indices_length, int branching, int level)
    {
        node->size = indices_length;
        node->level = level;

        if (indices_length < branching) {
            node->indices = indices;
            std::sort(node->indices,node->indices+indices_length);
            node->childs = NULL;
            return;
        }

        int* centers_idx = new int[branching];
        int centers_length;
        (this->*chooseCenters)(branching, indices, indices_length, centers_idx, centers_length);

        if (centers_length<branching) {
            node->indices = indices;
            std::sort(node->indices,node->indices+indices_length);
            node->childs = NULL;
            delete [] centers_idx;
            return;
        }


        Matrix<double> dcenters(new double[branching*veclen_],branching,veclen_);
        for (int i=0; i<centers_length; ++i) {
            ElementType* vec = dataset[centers_idx[i]];
            for (size_t k=0; k<veclen_; ++k) {
                dcenters[i][k] = double(vec[k]);
            }
        }
        delete[] centers_idx;

        float* radiuses = new float[branching];
        int* count = new int[branching];
        for (int i=0; i<branching; ++i) {
            radiuses[i] = 0;
            count[i] = 0;
        }

        //	assign points to clusters
        int* belongs_to = new int[indices_length];
        for (int i=0; i<indices_length; ++i) {

            float sq_dist = distance(dataset[indices[i]], dcenters[0], veclen_);
            belongs_to[i] = 0;
            for (int j=1; j<branching; ++j) {
                float new_sq_dist = distance(dataset[indices[i]], dcenters[j], veclen_);
                if (sq_dist>new_sq_dist) {
                    belongs_to[i] = j;
                    sq_dist = new_sq_dist;
                }
            }
            if (sq_dist>radiuses[belongs_to[i]]) {
                radiuses[belongs_to[i]] = sq_dist;
            }
            count[belongs_to[i]]++;
        }

        bool converged = false;
        int iteration = 0;
        while (!converged && iteration<max_iter) {
            converged = true;
            iteration++;

            // compute the new cluster centers
            for (int i=0; i<branching; ++i) {
                memset(dcenters[i],0,sizeof(double)*veclen_);
                radiuses[i] = 0;
            }
            for (int i=0; i<indices_length; ++i) {
                ElementType* vec = dataset[indices[i]];
                double* center = dcenters[belongs_to[i]];
                for (size_t k=0; k<veclen_; ++k) {
                    center[k] += vec[k];
                }
            }
            for (int i=0; i<branching; ++i) {
                int cnt = count[i];
                for (size_t k=0; k<veclen_; ++k) {
                    dcenters[i][k] /= cnt;
                }
            }

            // reassign points to clusters
            for (int i=0; i<indices_length; ++i) {
                float sq_dist = distance(dataset[indices[i]], dcenters[0], veclen_);
                int new_centroid = 0;
                for (int j=1; j<branching; ++j) {
                    float new_sq_dist = distance(dataset[indices[i]], dcenters[j], veclen_);
                    if (sq_dist>new_sq_dist) {
                        new_centroid = j;
                        sq_dist = new_sq_dist;
                    }
                }
                if (sq_dist>radiuses[new_centroid]) {
                    radiuses[new_centroid] = sq_dist;
                }
                if (new_centroid != belongs_to[i]) {
                    count[belongs_to[i]]--;
                    count[new_centroid]++;
                    belongs_to[i] = new_centroid;

                    converged = false;
                }
            }

            for (int i=0; i<branching; ++i) {
                // if one cluster converges to an empty cluster,
                // move an element into that cluster
                if (count[i]==0) {
                    int j = (i+1)%branching;
                    while (count[j]<=1) {
                        j = (j+1)%branching;
                    }

                    for (int k=0; k<indices_length; ++k) {
                        if (belongs_to[k]==j) {
                            belongs_to[k] = i;
                            count[j]--;
                            count[i]++;
                            break;
                        }
                    }
                    converged = false;
                }
            }

        }

        DistanceType** centers = new DistanceType*[branching];

        for (int i=0; i<branching; ++i) {
            centers[i] = new DistanceType[veclen_];
            memoryCounter += veclen_*sizeof(DistanceType);
            for (size_t k=0; k<veclen_; ++k) {
                centers[i][k] = dcenters[i][k];
            }
        }


        // compute kmeans clustering for each of the resulting clusters
        node->childs = pool.allocate<KMeansNodePtr>(branching);
        int start = 0;
        int end = start;
        for (int c=0; c<branching; ++c) {
            int s = count[c];

            float variance = 0;
            float mean_radius =0;
            for (int i=0; i<indices_length; ++i) {
                if (belongs_to[i]==c) {
                    float d = distance(dataset[indices[i]], ZeroIterator<ElementType>(), veclen_);
                    variance += d;
                    mean_radius += sqrt(d);
                    std::swap(indices[i],indices[end]);
                    std::swap(belongs_to[i],belongs_to[end]);
                    end++;
                }
            }
            variance /= s;
            mean_radius /= s;
            variance -= distance(centers[c], ZeroIterator<ElementType>(), veclen_);

            node->childs[c] = pool.allocate<KMeansNode>();
            node->childs[c]->radius = radiuses[c];
            node->childs[c]->pivot = centers[c];
            node->childs[c]->variance = variance;
            node->childs[c]->mean_radius = mean_radius;
            node->childs[c]->indices = NULL;
            computeClustering(node->childs[c],indices+start, end-start, branching, level+1);
            start=end;
        }

        delete[] dcenters.data;
        delete[] centers;
        delete[] radiuses;
        delete[] count;
        delete[] belongs_to;
    }



    /**
     * Performs one descent in the hierarchical k-means tree. The branches not
     * visited are stored in a priority queue.
     *
     * Params:
     *      node = node to explore
     *      result = container for the k-nearest neighbors found
     *      vec = query points
     *      checks = how many points in the dataset have been checked so far
     *      maxChecks = maximum dataset points to checks
     */


    void findNN(KMeansNodePtr node, ResultSet<DistanceType>& result, const ElementType* vec, int& checks, int maxChecks,
                Heap<BranchSt>* heap)
    {
        // Ignore those clusters that are too far away
        {
            DistanceType bsq = distance(vec, node->pivot, veclen_);
            DistanceType rsq = node->radius;
            DistanceType wsq = result.worstDist();

            DistanceType val = bsq-rsq-wsq;
            DistanceType val2 = val*val-4*rsq*wsq;

            //if (val>0) {
            if ((val>0)&&(val2>0)) {
                return;
            }
        }

        if (node->childs==NULL) {
            if (checks>=maxChecks) {
                if (result.full()) return;
            }
            checks += node->size;
            DistanceType worst_dist = result.worstDist();
            for (int i=0; i<node->size; ++i) {
                int index = node->indices[i];
                DistanceType dist = distance(dataset[index], vec, veclen_);
                if (dist<worst_dist) {
                    result.addPoint(dist, index);
                }
            }
        }
        else {
            float* domain_distances = new float[branching];
            int closest_center = exploreNodeBranches(node, vec, domain_distances, heap);
            delete[] domain_distances;
            findNN(node->childs[closest_center],result,vec, checks, maxChecks, heap);
        }
    }

    /**
     * Helper function that computes the nearest childs of a node to a given query point.
     * Params:
     *     node = the node
     *     q = the query point
     *     distances = array with the distances to each child node.
     * Returns:
     */
    int exploreNodeBranches(KMeansNodePtr node, const ElementType* q, float* domain_distances, Heap<BranchSt>* heap)
    {

        int best_index = 0;
        domain_distances[best_index] = distance(q, node->childs[best_index]->pivot, veclen_);
        for (int i=1; i<branching; ++i) {
            domain_distances[i] = distance(q, node->childs[i]->pivot, veclen_);
            if (domain_distances[i]<domain_distances[best_index]) {
                best_index = i;
            }
        }

        //		float* best_center = node->childs[best_index]->pivot;
        for (int i=0; i<branching; ++i) {
            if (i != best_index) {
                domain_distances[i] -= cb_index*node->childs[i]->variance;

                //				float dist_to_border = getDistanceToBorder(node.childs[i].pivot,best_center,q);
                //				if (domain_distances[i]<dist_to_border) {
                //					domain_distances[i] = dist_to_border;
                //				}
                heap->insert(BranchSt(node->childs[i],domain_distances[i]));
            }
        }

        return best_index;
    }


    /**
     * Function the performs exact nearest neighbor search by traversing the entire tree.
     */
    void findExactNN(KMeansNodePtr node, ResultSet<DistanceType>& result, const ElementType* vec)
    {
        // Ignore those clusters that are too far away
        {
            float bsq = distance(vec, node->pivot, veclen_);
            float rsq = node->radius;
            float wsq = result.worstDist();

            float val = bsq-rsq-wsq;
            float val2 = val*val-4*rsq*wsq;

            //                  if (val>0) {
            if ((val>0)&&(val2>0)) {
                return;
            }
        }


        if (node->childs==NULL) {
            DistanceType worst_dist = result.worstDist();
            for (int i=0; i<node->size; ++i) {
                int index = node->indices[i];
                DistanceType dist = distance(dataset[index], vec, veclen_);
                if (dist<worst_dist) {
                    result.addPoint(dist, index);
                }
            }
        }
        else {
            int* sort_indices = new int[branching];

            getCenterOrdering(node, vec, sort_indices);

            for (int i=0; i<branching; ++i) {
                findExactNN(node->childs[sort_indices[i]],result,vec);
            }

            delete[] sort_indices;
        }
    }


    /**
     * Helper function.
     *
     * I computes the order in which to traverse the child nodes of a particular node.
     */
    void getCenterOrdering(KMeansNodePtr node, const ElementType* q, int* sort_indices)
    {
        float* domain_distances = new float[branching];
        for (int i=0; i<branching; ++i) {
            float dist = distance(q, node->childs[i]->pivot, veclen_);

            int j=0;
            while (domain_distances[j]<dist && j<i) j++;
            for (int k=i; k>j; --k) {
                domain_distances[k] = domain_distances[k-1];
                sort_indices[k] = sort_indices[k-1];
            }
            domain_distances[j] = dist;
            sort_indices[j] = i;
        }
        delete[] domain_distances;
    }

    /**
     * Method that computes the squared distance from the query point q
     * from inside region with center c to the border between this
     * region and the region with center p
     */
    float getDistanceToBorder(float* p, float* c, float* q)
    {
        float sum = 0;
        float sum2 = 0;

        for (int i=0; i<veclen_; ++i) {
            float t = c[i]-p[i];
            sum += t*(q[i]-(c[i]+p[i])/2);
            sum2 += t*t;
        }

        return sum*sum/sum2;
    }


    /**
     * Helper function the descends in the hierarchical k-means tree by spliting those clusters that minimize
     * the overall variance of the clustering.
     * Params:
     *     root = root node
     *     clusters = array with clusters centers (return value)
     *     varianceValue = variance of the clustering (return value)
     * Returns:
     */
    int getMinVarianceClusters(KMeansNodePtr root, KMeansNodePtr* clusters, int clusters_length, float& varianceValue)
    {
        int clusterCount = 1;
        clusters[0] = root;

        float meanVariance = root->variance*root->size;

        while (clusterCount<clusters_length) {
            float minVariance = (std::numeric_limits<float>::max)();
            int splitIndex = -1;

            for (int i=0; i<clusterCount; ++i) {
                if (clusters[i]->childs != NULL) {

                    float variance = meanVariance - clusters[i]->variance*clusters[i]->size;

                    for (int j=0; j<branching; ++j) {
                        variance += clusters[i]->childs[j]->variance*clusters[i]->childs[j]->size;
                    }
                    if (variance<minVariance) {
                        minVariance = variance;
                        splitIndex = i;
                    }
                }
            }

            if (splitIndex==-1) break;
            if ( (branching+clusterCount-1) > clusters_length) break;

            meanVariance = minVariance;

            // split node
            KMeansNodePtr toSplit = clusters[splitIndex];
            clusters[splitIndex] = toSplit->childs[0];
            for (int i=1; i<branching; ++i) {
                clusters[clusterCount++] = toSplit->childs[i];
            }
        }

        varianceValue = meanVariance/root->size;
        return clusterCount;
    }
};

}

#endif //KMEANSTREE_H
