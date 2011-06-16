/**
 * @file neighbor_search.cc
 *
 * Implementation of AllkNN class to perform all-nearest-neighbors on two
 * specified data sets.
 */
#ifndef __MLPACK_NEIGHBOR_SEARCH_IMPL_H
#define __MLPACK_NEIGHBOR_SEARCH_IMPL_H

#include <fastlib/fx/io.h>

using namespace mlpack::neighbor;

// We call an advanced constructor of arma::mat which allows us to alias a
// matrix (if the user has asked for that).
template<typename Kernel, typename SortPolicy>
NeighborSearch<Kernel, SortPolicy>::NeighborSearch(arma::mat& queries_in,
                                                   arma::mat& references_in,
                                                   bool alias_matrix,
                                                   Kernel kernel) :
    references_(references_in.memptr(), references_in.n_rows,
        references_in.n_cols, !alias_matrix),
    queries_(queries_in.memptr(), queries_in.n_rows, queries_in.n_cols,
        !alias_matrix),
    kernel_(kernel),
    naive_(IO::GetParam<bool>("neighbor_search/naive_mode")),
    dual_mode_(!(naive_ || IO::GetParam<bool>("neighbor_search/single_mode"))),
    number_of_prunes_(0) {

  // C++0x will allow us to call out to other constructors so we can avoid this
  // copypasta problem.

  // Get the leaf size; naive ensures that the entire tree is one node
  if(naive_)
    leaf_size_ = max(queries_.n_cols, references_.n_cols);
  else
    leaf_size_ = IO::GetParam<int>("neighbor_search/leaf_size");

  // Make sure the leaf size is valid
  DEBUG_ASSERT(leaf_size_ > 0);

  // The data sets need to have the same number of points
  DEBUG_SAME_SIZE(queries_.n_rows, references_.n_rows);

  // K-nearest neighbors initialization
  knns_ = IO::GetParam<int>("neighbor_search/k");

  // Initialize the list of nearest neighbor candidates
  neighbor_indices_.set_size(knns_, queries_.n_cols);

  // Initialize the vector of upper bounds for each point.
  neighbor_distances_.set_size(knns_, queries_.n_cols);
  neighbor_distances_.fill(SortPolicy::WorstDistance());

  // We'll time tree building
  IO::StartTimer("neighbor_search/tree_building");

  // This call makes each tree from a matrix, leaf size, and two arrays
  // that record the permutation of the data points
  if (dual_mode_)
    query_tree_ = tree::MakeKdTreeMidpoint<TreeType>(queries_, leaf_size_,
        old_from_new_queries_);
  else
    query_tree_ = tree::MakeKdTreeMidpoint<TreeType>(queries_, queries_.n_cols,
        old_from_new_queries_);

  reference_tree_ = tree::MakeKdTreeMidpoint<TreeType>(references_,
      leaf_size_, old_from_new_references_);

  // Stop the timer we started above
  IO::StopTimer("neighbor_search/tree_building");
}

// We call an advanced constructor of arma::mat which allows us to alias a
// matrix (if the user has asked for that).
template<typename Kernel, typename SortPolicy>
NeighborSearch<Kernel, SortPolicy>::NeighborSearch(arma::mat& references_in,
                                                   bool alias_matrix,
                                                   Kernel kernel) :
    references_(references_in.memptr(), references_in.n_rows,
        references_in.n_cols, !alias_matrix),
    queries_(references_.memptr(), references_.n_rows, references_.n_cols,
        false),
    kernel_(kernel),
    naive_(IO::GetParam<bool>("neighbor_search/naive_mode")),
    dual_mode_(!(naive_ || IO::GetParam<bool>("neighbor_search/single_mode"))),
    number_of_prunes_(0) {

  // Get the leaf size from the module
  if(naive_)
    leaf_size_ = max(queries_.n_cols, references_.n_cols);
  else
    leaf_size_ = IO::GetParam<int>("neighbor_search/leaf_size");

  // Make sure the leaf size is valid
  DEBUG_ASSERT(leaf_size_ > 0);

  // K-nearest neighbors initialization
  knns_ = IO::GetParam<int>("neighbor_search/k");

  // Initialize the list of nearest neighbor candidates
  neighbor_indices_.set_size(knns_, references_.n_cols);

  // Initialize the vector of upper bounds for each point.
  neighbor_distances_.set_size(knns_, references_.n_cols);
  neighbor_distances_.fill(SortPolicy::WorstDistance());

  // We'll time tree building
  IO::StartTimer("neighbor_search/tree_building");

  // This call makes each tree from a matrix, leaf size, and two arrays
  // that record the permutation of the data points
  // Instead of NULL, it is possible to specify an array new_from_old_
  query_tree_ = NULL;
  reference_tree_ = tree::MakeKdTreeMidpoint<TreeType>(references_,
      leaf_size_, old_from_new_references_);

  // Stop the timer we started above
  IO::StopTimer("neighbor_search/tree_building");
}

/**
 * The tree is the only member we are responsible for deleting.  The others will
 * take care of themselves.
 */
template<typename Kernel, typename SortPolicy>
NeighborSearch<Kernel, SortPolicy>::~NeighborSearch() {
  if (reference_tree_ != query_tree_)
    delete reference_tree_;
  if (query_tree_ != NULL)
    delete query_tree_;
}

/**
 * Performs exhaustive computation between two leaves.
 */
template<typename Kernel, typename SortPolicy>
void NeighborSearch<Kernel, SortPolicy>::ComputeBaseCase_(
      TreeType* query_node,
      TreeType* reference_node) {

  // Check that the pointers are not NULL
  DEBUG_ASSERT(query_node != NULL);
  DEBUG_ASSERT(reference_node != NULL);

  // Check that we really should be in the base case
  DEBUG_WARN_IF(!query_node->is_leaf());
  DEBUG_WARN_IF(!reference_node->is_leaf());

  // Used to find the query node's new upper bound
  double query_max_neighbor_distance = SortPolicy::BestDistance();

  // node->begin() is the index of the first point in the node,
  // node->end is one past the last index
  for (index_t query_index = query_node->begin();
      query_index < query_node->end(); query_index++) {

    // Get the query point from the matrix
    arma::vec query_point = queries_.unsafe_col(query_index);

    double query_to_node_distance =
      SortPolicy::BestPointToNodeDistance(query_point, reference_node);

    if (SortPolicy::IsBetter(query_to_node_distance,
        neighbor_distances_(knns_ - 1, query_index))) {
      // We'll do the same for the references
      for (index_t reference_index = reference_node->begin();
          reference_index < reference_node->end(); reference_index++) {

        // Confirm that points do not identify themselves as neighbors
        // in the monochromatic case
        if (reference_node != query_node || reference_index != query_index) {
          arma::vec reference_point = references_.unsafe_col(reference_index);

          double distance = kernel_.Evaluate(query_point, reference_point);

          // If the reference point is closer than any of the current
          // candidates, add it to the list.
          arma::vec query_dist = neighbor_distances_.unsafe_col(query_index);
          index_t insert_position =  SortPolicy::SortDistance(query_dist,
              distance);

          if (insert_position != (index_t() - 1)) {
            InsertNeighbor(query_index, insert_position, reference_index,
                distance);
          }
        }
      }
    }

    // We need to find the upper bound distance for this query node
    if (SortPolicy::IsBetter(query_max_neighbor_distance,
        neighbor_distances_(knns_ - 1, query_index)))
      query_max_neighbor_distance = neighbor_distances_(knns_ - 1, query_index);
  }

  // Update the upper bound for the query_node
  query_node->stat().set_max_distance_so_far(query_max_neighbor_distance);

} // ComputeBaseCase_

/**
 * The recursive function for dual tree
 */
template<typename Kernel, typename SortPolicy>
void NeighborSearch<Kernel, SortPolicy>::ComputeDualNeighborsRecursion_(
      TreeType* query_node,
      TreeType* reference_node,
      double lower_bound) {

  if (SortPolicy::IsBetter(query_node->stat().max_distance_so_far(),
      lower_bound)) {
    number_of_prunes_++; // Pruned by distance; the nodes cannot be any closer
    return;              // than the already established lower bound.
  }

  if (query_node->is_leaf() && reference_node->is_leaf()) {
    ComputeBaseCase_(query_node, reference_node); // Base case: both are leaves.
    return;
  }

  if (query_node->is_leaf()) {
    // We must keep descending down the reference node to get to a leaf.

    // We'll order the computation by distance; descend in the direction of less
    // distance first.
    double left_distance = SortPolicy::BestNodeToNodeDistance(query_node,
        reference_node->left());
    double right_distance = SortPolicy::BestNodeToNodeDistance(query_node,
        reference_node->right());

    if (SortPolicy::IsBetter(left_distance, right_distance)) {
      ComputeDualNeighborsRecursion_(query_node, reference_node->left(),
          left_distance);
      ComputeDualNeighborsRecursion_(query_node, reference_node->right(),
          right_distance);
    } else {
      ComputeDualNeighborsRecursion_(query_node, reference_node->right(),
          right_distance);
      ComputeDualNeighborsRecursion_(query_node, reference_node->left(),
          left_distance);
    }
    return;
  }

  if (reference_node->is_leaf()) {
    // We must descend down the query node to get to a leaf.
    double left_distance = SortPolicy::BestNodeToNodeDistance(
        query_node->left(), reference_node);
    double right_distance = SortPolicy::BestNodeToNodeDistance(
        query_node->right(), reference_node);

    ComputeDualNeighborsRecursion_(query_node->left(), reference_node,
        left_distance);
    ComputeDualNeighborsRecursion_(query_node->right(), reference_node,
        right_distance);

    // We need to update the upper bound based on the new upper bounds of
    // the children
    double left_bound = query_node->left()->stat().max_distance_so_far();
    double right_bound = query_node->right()->stat().max_distance_so_far();

    if (SortPolicy::IsBetter(left_bound, right_bound))
      query_node->stat().set_max_distance_so_far(right_bound);
    else
      query_node->stat().set_max_distance_so_far(left_bound);

    return;
  }

  // Neither side is a leaf; so we recurse on all combinations of both.  The
  // calculations are ordered by distance.
  double left_distance = SortPolicy::BestNodeToNodeDistance(query_node->left(),
      reference_node->left());
  double right_distance = SortPolicy::BestNodeToNodeDistance(query_node->left(),
      reference_node->right());

  // Recurse on query_node->left() first.
  if (SortPolicy::IsBetter(left_distance, right_distance)) {
    ComputeDualNeighborsRecursion_(query_node->left(), reference_node->left(),
        left_distance);
    ComputeDualNeighborsRecursion_(query_node->left(), reference_node->right(),
        right_distance);
  } else {
    ComputeDualNeighborsRecursion_(query_node->left(), reference_node->right(),
        right_distance);
    ComputeDualNeighborsRecursion_(query_node->left(), reference_node->left(),
        left_distance);
  }

  left_distance = SortPolicy::BestNodeToNodeDistance(query_node->right(),
      reference_node->left());
  right_distance = SortPolicy::BestNodeToNodeDistance(query_node->right(),
      reference_node->right());

  // Now recurse on query_node->right().
  if (SortPolicy::IsBetter(left_distance, right_distance)) {
    ComputeDualNeighborsRecursion_(query_node->right(), reference_node->left(),
        left_distance);
    ComputeDualNeighborsRecursion_(query_node->right(), reference_node->right(),
        right_distance);
  } else {
    ComputeDualNeighborsRecursion_(query_node->right(), reference_node->right(),
        right_distance);
    ComputeDualNeighborsRecursion_(query_node->right(), reference_node->left(),
        left_distance);
  }

  // Update the upper bound as above
  double left_bound = query_node->left()->stat().max_distance_so_far();
  double right_bound = query_node->right()->stat().max_distance_so_far();

  if (SortPolicy::IsBetter(left_bound, right_bound))
    query_node->stat().set_max_distance_so_far(right_bound);
  else
    query_node->stat().set_max_distance_so_far(left_bound);

} // ComputeDualNeighborsRecursion_

template<typename Kernel, typename SortPolicy>
void NeighborSearch<Kernel, SortPolicy>::ComputeSingleNeighborsRecursion_(
      index_t point_id,
      arma::vec& point,
      TreeType* reference_node,
      double& best_dist_so_far) {

  if (reference_node->is_leaf()) {
    // Base case: reference node is a leaf
    
    for (index_t reference_index = reference_node->begin();
        reference_index < reference_node->end(); reference_index++) {
      // Confirm that points do not identify themselves as neighbors
      // in the monochromatic case
      if (!(references_.memptr() == queries_.memptr() &&
            reference_index == point_id)) {
        arma::vec reference_point = references_.unsafe_col(reference_index);

        double distance = kernel_.Evaluate(point, reference_point);

        // If the reference point is better than any of the current candidates,
        // insert it into the list correctly.
        arma::vec query_dist = neighbor_distances_.unsafe_col(point_id);
        index_t insert_position =  SortPolicy::SortDistance(query_dist,
            distance);

        if (insert_position != (index_t() - 1))
          InsertNeighbor(point_id, insert_position, reference_index, distance);
      }
    } // for reference_index
    
    best_dist_so_far = neighbor_distances_(knns_ - 1, point_id);
  } else {
    // We'll order the computation by distance.
    double left_distance = SortPolicy::BestPointToNodeDistance(point,
        reference_node->left());
    double right_distance = SortPolicy::BestPointToNodeDistance(point,
        reference_node->right());

    // Recurse in the best direction first.
    if (SortPolicy::IsBetter(left_distance, right_distance)) {
      if (SortPolicy::IsBetter(best_dist_so_far, left_distance))
        number_of_prunes_++; // Prune; no possibility of finding a better point.
      else
        ComputeSingleNeighborsRecursion_(point_id, point,
            reference_node->left(), best_dist_so_far);

      if (SortPolicy::IsBetter(best_dist_so_far, right_distance))
        number_of_prunes_++; // Prune; no possibility of finding a better point.
      else
        ComputeSingleNeighborsRecursion_(point_id, point,
            reference_node->right(), best_dist_so_far);
      
    } else {
      if (SortPolicy::IsBetter(best_dist_so_far, right_distance))
        number_of_prunes_++; // Prune; no possibility of finding a better point.
      else
        ComputeSingleNeighborsRecursion_(point_id, point,
            reference_node->right(), best_dist_so_far);

      if (SortPolicy::IsBetter(best_dist_so_far, left_distance))
        number_of_prunes_++; // Prune; no possibility of finding a better point.
      else
        ComputeSingleNeighborsRecursion_(point_id, point,
            reference_node->left(), best_dist_so_far);
    }
  }
}

/**
 * Computes the best neighbors and stores them in resulting_neighbors and
 * distances.
 */
template<typename Kernel, typename SortPolicy>
void NeighborSearch<Kernel, SortPolicy>::ComputeNeighbors(
      arma::Mat<index_t>& resulting_neighbors,
      arma::mat& distances) {

  IO::StartTimer("neighbor_search/computing_neighbors");
  if (naive_) {
    // Run the base case computation on all nodes
    if (query_tree_)
      ComputeBaseCase_(query_tree_, reference_tree_);
    else
      ComputeBaseCase_(reference_tree_, reference_tree_);
  } else {
    if (dual_mode_) {
      // Start on the root of each tree
      if (query_tree_) {
        ComputeDualNeighborsRecursion_(query_tree_, reference_tree_,
            SortPolicy::BestNodeToNodeDistance(query_tree_, reference_tree_));
      } else {
        ComputeDualNeighborsRecursion_(reference_tree_, reference_tree_,
            SortPolicy::BestNodeToNodeDistance(reference_tree_,
            reference_tree_));
      }
    } else {
      index_t chunk = queries_.n_cols / 10;

      for(index_t i = 0; i < 10; i++) {
        for(index_t j = 0; j < chunk; j++) {
          arma::vec point = queries_.unsafe_col(i * chunk + j);
          double best_dist_so_far = SortPolicy::WorstDistance();
          ComputeSingleNeighborsRecursion_(i * chunk + j, point,
              reference_tree_, best_dist_so_far);
        }
      }
      for(index_t i = 0; i < queries_.n_cols % 10; i++) {
        index_t ind = (queries_.n_cols / 10) * 10 + i;
        arma::vec point = queries_.unsafe_col(ind);
        double best_dist_so_far = SortPolicy::WorstDistance();
        ComputeSingleNeighborsRecursion_(ind, point, reference_tree_,
            best_dist_so_far);
      }
    }
  }

  IO::StopTimer("neighbor_search/computing_neighbors");

  // We need to initialize the results list before filling it
  resulting_neighbors.set_size(neighbor_indices_.n_rows,
      neighbor_indices_.n_cols);
  distances.set_size(neighbor_distances_.n_rows, neighbor_distances_.n_cols);

  // We need to map the indices back from how they have been permuted
  if (query_tree_ != NULL) {
    for (index_t i = 0; i < neighbor_indices_.n_cols; i++) {
      for (index_t k = 0; k < neighbor_indices_.n_rows; k++) {
        resulting_neighbors(k, old_from_new_queries_[i]) =
            old_from_new_references_[neighbor_indices_(k, i)];
        distances(k, old_from_new_queries_[i]) = neighbor_distances_(k, i);
      }
    }
  } else {
    for (index_t i = 0; i < neighbor_indices_.n_cols; i++) {
      for (index_t k = 0; k < neighbor_indices_.n_rows; k++) {
        resulting_neighbors(k, old_from_new_references_[i]) =
            old_from_new_references_[neighbor_indices_(k, i)];
        distances(k, old_from_new_references_[i]) = neighbor_distances_(k, i);
      }
    }
  }
} // ComputeNeighbors

/***
 * Helper function to insert a point into the neighbors and distances matrices.
 *
 * @param query_index Index of point whose neighbors we are inserting into.
 * @param pos Position in list to insert into.
 * @param neighbor Index of reference point which is being inserted.
 * @param distance Distance from query point to reference point.
 */
template<typename Kernel, typename SortPolicy>
void NeighborSearch<Kernel, SortPolicy>::InsertNeighbor(index_t query_index,
                                                        index_t pos,
                                                        index_t neighbor,
                                                        double distance) {
  // We only memmove() if there is actually a need to shift something.
  if (pos < (knns_ - 1)) {
    int len = (knns_ - 1) - pos;
    memmove(neighbor_distances_.colptr(query_index) + (pos + 1),
        neighbor_distances_.colptr(query_index) + pos,
        sizeof(double) * len);
    memmove(neighbor_indices_.colptr(query_index) + (pos + 1),
        neighbor_indices_.colptr(query_index) + pos,
        sizeof(index_t) * len);
  }

  // Now put the new information in the right index.
  neighbor_distances_(pos, query_index) = distance;
  neighbor_indices_(pos, query_index) = neighbor;
}

#endif