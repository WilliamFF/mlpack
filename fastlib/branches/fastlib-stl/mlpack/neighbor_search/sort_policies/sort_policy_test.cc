/***
 * @file sort_policy_test.cc
 * @author Ryan Curtin
 *
 * Tests for each of the implementations of the SortPolicy class.
 */
#include <fastlib/fastlib.h>
#include <armadillo>

// Classes to test.
#include "nearest_neighbor_sort.h"
#include "furthest_neighbor_sort.h"

#define BOOST_TEST_MODULE SortPolicy Test
#include <boost/test/unit_test.hpp>

using namespace mlpack;
using namespace mlpack::neighbor;

// Tests for NearestNeighborSort

/***
 * Ensure the best distance for nearest neighbors is 0.
 */
BOOST_AUTO_TEST_CASE(nns_best_distance) {
  BOOST_REQUIRE(NearestNeighborSort::BestDistance() == 0);
}

/***
 * Ensure the worst distance for nearest neighbors is DBL_MAX.
 */
BOOST_AUTO_TEST_CASE(nns_worst_distance) {
  BOOST_REQUIRE(NearestNeighborSort::WorstDistance() == DBL_MAX);
}

/***
 * Make sure the comparison works for values strictly less than the reference.
 */
BOOST_AUTO_TEST_CASE(nns_is_better_strict) {
  BOOST_REQUIRE(NearestNeighborSort::IsBetter(5.0, 6.0) == true);
}

/***
 * Warn in case the comparison is not strict.
 */
BOOST_AUTO_TEST_CASE(nns_is_better_not_strict) {
  BOOST_WARN(NearestNeighborSort::IsBetter(6.0, 6.0) == true);
}

/***
 * A simple test case of where to insert when all the values in the list are
 * DBL_MAX.
 */
BOOST_AUTO_TEST_CASE(nns_sort_distance_all_dbl_max) {
  arma::vec list(5);
  list.fill(DBL_MAX);

  // Should be inserted at the head of the list.
  BOOST_REQUIRE(NearestNeighborSort::SortDistance(list, 5.0) == 0);
}

/***
 * Another test case, where we are just putting the new value in the middle of
 * the list.
 */
BOOST_AUTO_TEST_CASE(nns_sort_distance_2) {
  arma::vec list(3);
  list[0] = 0.66;
  list[1] = 0.89;
  list[2] = 1.14;

  // Run a couple possibilities through.
  BOOST_REQUIRE(NearestNeighborSort::SortDistance(list, 0.61) == 0);
  BOOST_REQUIRE(NearestNeighborSort::SortDistance(list, 0.76) == 1);
  BOOST_REQUIRE(NearestNeighborSort::SortDistance(list, 0.99) == 2);
  BOOST_REQUIRE(NearestNeighborSort::SortDistance(list, 1.22) ==
      (index_t() - 1));
}

/***
 * Very simple sanity check to ensure that bounds are working alright.  We will
 * use a one-dimensional bound for simplicity.
 */
BOOST_AUTO_TEST_CASE(nns_node_to_node_distance) {
  // Well, there's no easy way to make DHrectBounds the way we want, so we have
  // to make them and then expand the region to include new points.
  BinarySpaceTree<DHrectBound<2>, arma::mat> node_one;
  arma::vec utility(1);
  utility[0] = 0;

  node_one.bound().SetSize(1);
  node_one.bound() |= utility;
  utility[0] = 1;
  node_one.bound() |= utility;

  BinarySpaceTree<DHrectBound<2>, arma::mat> node_two;
  node_two.bound().SetSize(1);

  utility[0] = 5;
  node_two.bound() |= utility;
  utility[0] = 6;
  node_two.bound() |= utility;

  // This should use the L2 squared distance.
  BOOST_REQUIRE_CLOSE(NearestNeighborSort::BestNodeToNodeDistance(&node_one,
      &node_two), 16.0, 1e-5);

  // And another just to be sure, from the other side.
  node_two.bound().Reset();
  utility[0] = -2;
  node_two.bound() |= utility;
  utility[0] = -1;
  node_two.bound() |= utility;

  // Again, the distance is the L2 squared distance.
  BOOST_REQUIRE_CLOSE(NearestNeighborSort::BestNodeToNodeDistance(&node_one,
      &node_two), 1.0, 1e-5);

  // Now, when the bounds overlap.
  node_two.bound().Reset();
  utility[0] = -0.5;
  node_two.bound() |= utility;
  utility[0] = 0.5;
  node_two.bound() |= utility;

  BOOST_REQUIRE_CLOSE(NearestNeighborSort::BestNodeToNodeDistance(&node_one,
      &node_two), 0.0, 1e-5);
}

/***
 * Another very simple sanity check for the point-to-node case, again in one
 * dimension.
 */
BOOST_AUTO_TEST_CASE(nns_point_to_node_distance) {
  // Well, there's no easy way to make DHrectBounds the way we want, so we have
  // to make them and then expand the region to include new points.
  arma::vec utility(1);
  utility[0] = 0;

  BinarySpaceTree<DHrectBound<2>, arma::mat> node;
  node.bound().SetSize(1);
  node.bound() |= utility;
  utility[0] = 1;
  node.bound() |= utility;

  arma::vec point(1);
  point[0] = -0.5;

  // The distance is the L2 squared distance.
  BOOST_REQUIRE_CLOSE(NearestNeighborSort::BestPointToNodeDistance(point,
      &node), 0.25, 1e-5);

  // Now from the other side of the bound.
  point[0] = 1.5;

  BOOST_REQUIRE_CLOSE(NearestNeighborSort::BestPointToNodeDistance(point,
      &node), 0.25, 1e-5);

  // And now when the point is inside the bound.
  point[0] = 0.5;

  BOOST_REQUIRE_CLOSE(NearestNeighborSort::BestPointToNodeDistance(point,
      &node), 0.0, 1e-5);
}

// Tests for FurthestNeighborSort

/***
 * Ensure the best distance for furthest neighbors is DBL_MAX.
 */
BOOST_AUTO_TEST_CASE(fns_best_distance) {
  BOOST_REQUIRE(FurthestNeighborSort::BestDistance() == DBL_MAX);
}

/***
 * Ensure the worst distance for furthest neighbors is 0.
 */
BOOST_AUTO_TEST_CASE(fns_worst_distance) {
  BOOST_REQUIRE(FurthestNeighborSort::WorstDistance() == 0);
}

/***
 * Make sure the comparison works for values strictly less than the reference.
 */
BOOST_AUTO_TEST_CASE(fns_is_better_strict) {
  BOOST_REQUIRE(FurthestNeighborSort::IsBetter(5.0, 4.0) == true);
}

/***
 * Warn in case the comparison is not strict.
 */
BOOST_AUTO_TEST_CASE(fns_is_better_not_strict) {
  BOOST_WARN(FurthestNeighborSort::IsBetter(6.0, 6.0) == true);
}

/***
 * A simple test case of where to insert when all the values in the list are
 * 0.
 */
BOOST_AUTO_TEST_CASE(fns_sort_distance_all_zero) {
  arma::vec list(5);
  list.fill(0);

  // Should be inserted at the head of the list.
  BOOST_REQUIRE(FurthestNeighborSort::SortDistance(list, 5.0) == 0);
}

/***
 * Another test case, where we are just putting the new value in the middle of
 * the list.
 */
BOOST_AUTO_TEST_CASE(fns_sort_distance_2) {
  arma::vec list(3);
  list[0] = 1.14;
  list[1] = 0.89;
  list[2] = 0.66;

  // Run a couple possibilities through.
  BOOST_REQUIRE(FurthestNeighborSort::SortDistance(list, 1.22) == 0);
  BOOST_REQUIRE(FurthestNeighborSort::SortDistance(list, 0.93) == 1);
  BOOST_REQUIRE(FurthestNeighborSort::SortDistance(list, 0.68) == 2);
  BOOST_REQUIRE(FurthestNeighborSort::SortDistance(list, 0.62) ==
      (index_t() - 1));
}

/***
 * Very simple sanity check to ensure that bounds are working alright.  We will
 * use a one-dimensional bound for simplicity.
 */
BOOST_AUTO_TEST_CASE(fns_node_to_node_distance) {
  // Well, there's no easy way to make DHrectBounds the way we want, so we have
  // to make them and then expand the region to include new points.
  arma::vec utility(1);
  utility[0] = 0;

  BinarySpaceTree<DHrectBound<2>, arma::mat> node_one;
  node_one.bound().SetSize(1);
  node_one.bound() |= utility;
  utility[0] = 1;
  node_one.bound() |= utility;

  BinarySpaceTree<DHrectBound<2>, arma::mat> node_two;
  node_two.bound().SetSize(1);
  utility[0] = 5;
  node_two.bound() |= utility;
  utility[0] = 6;
  node_two.bound() |= utility;

  // This should use the L2 squared distance.
  BOOST_REQUIRE_CLOSE(FurthestNeighborSort::BestNodeToNodeDistance(&node_one,
      &node_two), 36.0, 1e-5);

  // And another just to be sure, from the other side.
  node_two.bound().Reset();
  utility[0] = -2;
  node_two.bound() |= utility;
  utility[0] = -1;
  node_two.bound() |= utility;

  // Again, the distance is the L2 squared distance.
  BOOST_REQUIRE_CLOSE(FurthestNeighborSort::BestNodeToNodeDistance(&node_one,
      &node_two), 9.0, 1e-5);

  // Now, when the bounds overlap.
  node_two.bound().Reset();
  utility[0] = -0.5;
  node_two.bound() |= utility;
  utility[0] = 0.5;
  node_two.bound() |= utility;

  BOOST_REQUIRE_CLOSE(FurthestNeighborSort::BestNodeToNodeDistance(&node_one,
      &node_two), (1.5 * 1.5), 1e-5);
}

/***
 * Another very simple sanity check for the point-to-node case, again in one
 * dimension.
 */
BOOST_AUTO_TEST_CASE(fns_point_to_node_distance) {
  // Well, there's no easy way to make DHrectBounds the way we want, so we have
  // to make them and then expand the region to include new points.
  arma::vec utility(1);
  utility[0] = 0;

  BinarySpaceTree<DHrectBound<2>, arma::mat> node;
  node.bound().SetSize(1);
  node.bound() |= utility;
  utility[0] = 1;
  node.bound() |= utility;

  arma::vec point(1);
  point[0] = -0.5;

  // The distance is the L2 squared distance.
  BOOST_REQUIRE_CLOSE(FurthestNeighborSort::BestPointToNodeDistance(point,
      &node), (1.5 * 1.5), 1e-5);

  // Now from the other side of the bound.
  point[0] = 1.5;

  BOOST_REQUIRE_CLOSE(FurthestNeighborSort::BestPointToNodeDistance(point,
      &node), (1.5 * 1.5), 1e-5);

  // And now when the point is inside the bound.
  point[0] = 0.5;

  BOOST_REQUIRE_CLOSE(FurthestNeighborSort::BestPointToNodeDistance(point,
      &node), 0.25, 1e-5);
}